#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "ds/dynamic_array.h"

namespace uc {

// FNV-1a 字符串哈希。
struct Fnv1aHash {
    std::size_t operator()(const std::string& text) const noexcept {
        std::uint64_t hash = 1469598103934665603ull;
        for (unsigned char c : text) {
            hash ^= static_cast<std::uint64_t>(c);
            hash *= 1099511628211ull;
        }
        return static_cast<std::size_t>(hash);
    }
};

// 整数混合哈希（MurmurHash3 finalizer 思路）。
struct IntHash {
    std::size_t operator()(std::uint64_t value) const noexcept {
        value ^= value >> 33;
        value *= 0xff51afd7ed558ccdull;
        value ^= value >> 33;
        value *= 0xc4ceb9fe1a85ec53ull;
        value ^= value >> 33;
        return static_cast<std::size_t>(value);
    }
};

// 自写哈希表：冲突采用链地址法，装载因子超过 0.75 时扩容。
template <class K, class V, class Hash>
class HashMap {
public:
    HashMap() { rehash(16); }

    explicit HashMap(std::size_t initialBuckets) {
        rehash(initialBuckets < 8 ? 8 : initialBuckets);
    }

    HashMap(const HashMap& other) {
        rehash(other.bucketCount_);
        for (std::size_t b = 0; b < other.bucketCount_; ++b) {
            for (Node* node = other.buckets_[b]; node != nullptr; node = node->next) {
                insert(node->key, node->value);
            }
        }
    }

    HashMap(HashMap&& other) noexcept
        : buckets_(other.buckets_),
          bucketCount_(other.bucketCount_),
          size_(other.size_),
          hasher_(other.hasher_) {
        other.buckets_ = nullptr;
        other.bucketCount_ = 0;
        other.size_ = 0;
    }

    HashMap& operator=(const HashMap& other) {
        if (this != &other) {
            HashMap tmp(other);
            swap(tmp);
        }
        return *this;
    }

    HashMap& operator=(HashMap&& other) noexcept {
        if (this != &other) {
            release();
            buckets_ = other.buckets_;
            bucketCount_ = other.bucketCount_;
            size_ = other.size_;
            hasher_ = other.hasher_;
            other.buckets_ = nullptr;
            other.bucketCount_ = 0;
            other.size_ = 0;
        }
        return *this;
    }

    ~HashMap() { release(); }

    std::size_t size() const { return size_; }
    std::size_t bucketCount() const { return bucketCount_; }
    bool empty() const { return size_ == 0; }

    bool insert(const K& key, const V& value) {
        if (findNode(key) != nullptr) {
            return false;
        }
        addNode(key, value);
        maybeGrow();
        return true;
    }

    // 存在则覆盖，返回是否为新增。
    bool set(const K& key, const V& value) {
        Node* node = findNode(key);
        if (node != nullptr) {
            node->value = value;
            return false;
        }
        addNode(key, value);
        maybeGrow();
        return true;
    }

    bool contains(const K& key) const { return findNode(key) != nullptr; }

    const V* find(const K& key) const {
        Node* node = findNode(key);
        return node == nullptr ? nullptr : &node->value;
    }

    V* find(const K& key) {
        Node* node = findNode(key);
        return node == nullptr ? nullptr : &node->value;
    }

    void clear() {
        if (buckets_ == nullptr) {
            return;
        }
        for (std::size_t b = 0; b < bucketCount_; ++b) {
            Node* node = buckets_[b];
            while (node != nullptr) {
                Node* next = node->next;
                delete node;
                node = next;
            }
            buckets_[b] = nullptr;
        }
        size_ = 0;
    }

    void swap(HashMap& other) noexcept {
        std::swap(buckets_, other.buckets_);
        std::swap(bucketCount_, other.bucketCount_);
        std::swap(size_, other.size_);
        std::swap(hasher_, other.hasher_);
    }

    template <class F>
    void forEach(F&& fn) const {
        if (buckets_ == nullptr) {
            return;
        }
        for (std::size_t b = 0; b < bucketCount_; ++b) {
            for (Node* node = buckets_[b]; node != nullptr; node = node->next) {
                fn(node->key, node->value);
            }
        }
    }

private:
    struct Node {
        K key;
        V value;
        Node* next;
    };

    Node* findNode(const K& key) const {
        if (buckets_ == nullptr || bucketCount_ == 0) {
            return nullptr;
        }
        std::size_t bucket = hasher_(key) % bucketCount_;
        for (Node* node = buckets_[bucket]; node != nullptr; node = node->next) {
            if (node->key == key) {
                return node;
            }
        }
        return nullptr;
    }

    void addNode(const K& key, const V& value) {
        std::size_t bucket = hasher_(key) % bucketCount_;
        Node* node = new Node{key, value, buckets_[bucket]};
        buckets_[bucket] = node;
        ++size_;
    }

    void maybeGrow() {
        if (size_ * 4 > bucketCount_ * 3) {
            rehash(bucketCount_ * 2);
        }
    }

    void rehash(std::size_t newBucketCount) {
        if (newBucketCount < 8) {
            newBucketCount = 8;
        }
        Node** newBuckets = new Node*[newBucketCount]();
        if (buckets_ != nullptr) {
            for (std::size_t b = 0; b < bucketCount_; ++b) {
                Node* node = buckets_[b];
                while (node != nullptr) {
                    Node* next = node->next;
                    std::size_t bucket = hasher_(node->key) % newBucketCount;
                    node->next = newBuckets[bucket];
                    newBuckets[bucket] = node;
                    node = next;
                }
            }
            delete[] buckets_;
        }
        buckets_ = newBuckets;
        bucketCount_ = newBucketCount;
    }

    void release() {
        clear();
        delete[] buckets_;
        buckets_ = nullptr;
        bucketCount_ = 0;
    }

    Node** buckets_ = nullptr;
    std::size_t bucketCount_ = 0;
    std::size_t size_ = 0;
    Hash hasher_{};
};

}  // namespace uc
