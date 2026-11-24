#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace uc {

// 自写动态数组：课程要求的数据结构之一。
// 线性表采用连续存储，支持容量自动增长，均摊插入复杂度 O(1)。
template <class T>
class DynamicArray {
public:
    DynamicArray() = default;

    explicit DynamicArray(std::size_t capacity) { reserve(capacity); }

    DynamicArray(const DynamicArray& other) { copyFrom(other); }

    DynamicArray(DynamicArray&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    DynamicArray& operator=(const DynamicArray& other) {
        if (this != &other) {
            DynamicArray tmp(other);
            swap(tmp);
        }
        return *this;
    }

    DynamicArray& operator=(DynamicArray&& other) noexcept {
        if (this != &other) {
            release();
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    ~DynamicArray() { release(); }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    std::size_t capacity() const { return capacity_; }

    T& operator[](std::size_t index) { return data_[index]; }
    const T& operator[](std::size_t index) const { return data_[index]; }

    T& at(std::size_t index) {
        if (index >= size_) {
            throw std::out_of_range("DynamicArray::at");
        }
        return data_[index];
    }

    const T& at(std::size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("DynamicArray::at");
        }
        return data_[index];
    }

    T& front() { return data_[0]; }
    const T& front() const { return data_[0]; }
    T& back() { return data_[size_ - 1]; }
    const T& back() const { return data_[size_ - 1]; }

    T* data() { return data_; }
    const T* data() const { return data_; }

    void push_back(const T& value) {
        ensureCapacity(size_ + 1);
        data_[size_++] = value;
    }

    void push_back(T&& value) {
        ensureCapacity(size_ + 1);
        data_[size_++] = std::move(value);
    }

    void pop_back() {
        if (size_ > 0) {
            --size_;
        }
    }

    void clear() { size_ = 0; }

    void reserve(std::size_t capacity) {
        if (capacity > capacity_) {
            reallocate(capacity);
        }
    }

    void resize(std::size_t size) {
        if (size > capacity_) {
            reallocate(size);
        }
        for (std::size_t i = size_; i < size; ++i) {
            data_[i] = T();
        }
        size_ = size;
    }

    void swap(DynamicArray& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

private:
    void ensureCapacity(std::size_t required) {
        if (required <= capacity_) {
            return;
        }
        std::size_t next = capacity_ == 0 ? 8 : capacity_ * 2;
        while (next < required) {
            next *= 2;
        }
        reallocate(next);
    }

    void reallocate(std::size_t newCapacity) {
        T* newData = new T[newCapacity];
        for (std::size_t i = 0; i < size_; ++i) {
            newData[i] = std::move(data_[i]);
        }
        delete[] data_;
        data_ = newData;
        capacity_ = newCapacity;
    }

    void copyFrom(const DynamicArray& other) {
        if (other.size_ == 0) {
            return;
        }
        data_ = new T[other.size_];
        for (std::size_t i = 0; i < other.size_; ++i) {
            data_[i] = other.data_[i];
        }
        size_ = other.size_;
        capacity_ = other.size_;
    }

    void release() {
        delete[] data_;
        data_ = nullptr;
        size_ = 0;
        capacity_ = 0;
    }

    T* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};

}  // namespace uc
