#pragma once

#include <cstddef>
#include <stdexcept>

#include "ds/dynamic_array.h"

namespace uc {

// 自写队列：用动态数组 + 头指针实现，定期压缩避免空间无限增长。
template <class T>
class Queue {
public:
    Queue() = default;

    bool empty() const { return head_ >= data_.size(); }
    std::size_t size() const { return data_.size() - head_; }

    void push(const T& value) { data_.push_back(value); }
    void push(T&& value) { data_.push_back(static_cast<T&&>(value)); }

    T& front() {
        if (empty()) {
            throw std::out_of_range("Queue::front on empty queue");
        }
        return data_[head_];
    }

    const T& front() const {
        if (empty()) {
            throw std::out_of_range("Queue::front on empty queue");
        }
        return data_[head_];
    }

    void pop() {
        if (empty()) {
            return;
        }
        ++head_;
        if (head_ > 64 && head_ * 2 >= data_.size()) {
            compact();
        }
    }

    void clear() {
        data_.clear();
        head_ = 0;
    }

private:
    void compact() {
        DynamicArray<T> compacted;
        compacted.reserve(size());
        for (std::size_t i = head_; i < data_.size(); ++i) {
            compacted.push_back(static_cast<T&&>(data_[i]));
        }
        data_ = static_cast<DynamicArray<T>&&>(compacted);
        head_ = 0;
    }

    DynamicArray<T> data_;
    std::size_t head_ = 0;
};

}  // namespace uc
