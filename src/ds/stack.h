#pragma once

#include <stdexcept>

#include "ds/dynamic_array.h"

namespace uc {

// 自写栈：用动态数组实现后进先出。
template <class T>
class Stack {
public:
    bool empty() const { return data_.empty(); }
    std::size_t size() const { return data_.size(); }

    void push(const T& value) { data_.push_back(value); }
    void push(T&& value) { data_.push_back(static_cast<T&&>(value)); }

    T& top() {
        if (empty()) {
            throw std::out_of_range("Stack::top on empty stack");
        }
        return data_.back();
    }

    const T& top() const {
        if (empty()) {
            throw std::out_of_range("Stack::top on empty stack");
        }
        return data_.back();
    }

    void pop() { data_.pop_back(); }

    void clear() { data_.clear(); }

private:
    DynamicArray<T> data_;
};

}  // namespace uc
