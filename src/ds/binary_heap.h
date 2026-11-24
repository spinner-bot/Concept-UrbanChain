#pragma once

#include <stdexcept>

#include "ds/dynamic_array.h"

namespace uc {

struct HeapItem {
    double key = 0.0;
    int value = -1;
};

// 自写二叉最小堆：完全二叉树顺序存储。
// 用于 Dijkstra、候选 Top-k 等场景。
class BinaryMinHeap {
public:
    BinaryMinHeap() = default;

    bool empty() const { return items_.empty(); }
    std::size_t size() const { return items_.size(); }

    void clear() { items_.clear(); }

    void push(double key, int value) {
        items_.push_back(HeapItem{key, value});
        siftUp(items_.size() - 1);
    }

    HeapItem pop() {
        if (items_.empty()) {
            throw std::out_of_range("BinaryMinHeap::pop on empty heap");
        }
        HeapItem top = items_[0];
        items_[0] = items_.back();
        items_.pop_back();
        if (!items_.empty()) {
            siftDown(0);
        }
        return top;
    }

    const HeapItem& top() const {
        if (items_.empty()) {
            throw std::out_of_range("BinaryMinHeap::top on empty heap");
        }
        return items_[0];
    }

private:
    static bool less(const HeapItem& a, const HeapItem& b) {
        if (a.key != b.key) {
            return a.key < b.key;
        }
        return a.value < b.value;
    }

    void siftUp(std::size_t index) {
        while (index > 0) {
            std::size_t parent = (index - 1) / 2;
            if (!less(items_[index], items_[parent])) {
                break;
            }
            swapItems(index, parent);
            index = parent;
        }
    }

    void siftDown(std::size_t index) {
        const std::size_t count = items_.size();
        while (true) {
            std::size_t left = index * 2 + 1;
            std::size_t right = left + 1;
            std::size_t smallest = index;
            if (left < count && less(items_[left], items_[smallest])) {
                smallest = left;
            }
            if (right < count && less(items_[right], items_[smallest])) {
                smallest = right;
            }
            if (smallest == index) {
                break;
            }
            swapItems(index, smallest);
            index = smallest;
        }
    }

    void swapItems(std::size_t a, std::size_t b) {
        HeapItem tmp = items_[a];
        items_[a] = items_[b];
        items_[b] = tmp;
    }

    DynamicArray<HeapItem> items_;
};

}  // namespace uc
