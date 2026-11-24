#pragma once

#include <utility>

#include "ds/dynamic_array.h"

namespace uc {

// 自写快速排序（三数取中 + 小区间插入排序），用于确定性输出与候选排序。
template <class T, class Less>
void insertionSortRange(DynamicArray<T>& values, int low, int high, Less less) {
    for (int i = low + 1; i <= high; ++i) {
        T value = values[static_cast<std::size_t>(i)];
        int j = i - 1;
        while (j >= low && less(value, values[static_cast<std::size_t>(j)])) {
            values[static_cast<std::size_t>(j + 1)] = values[static_cast<std::size_t>(j)];
            --j;
        }
        values[static_cast<std::size_t>(j + 1)] = value;
    }
}

template <class T, class Less>
void quickSortRange(DynamicArray<T>& values, int low, int high, Less less) {
    while (low < high) {
        if (high - low < 16) {
            insertionSortRange(values, low, high, less);
            return;
        }
        const int mid = low + (high - low) / 2;
        if (less(values[static_cast<std::size_t>(mid)],
                 values[static_cast<std::size_t>(low)])) {
            std::swap(values[static_cast<std::size_t>(mid)],
                      values[static_cast<std::size_t>(low)]);
        }
        if (less(values[static_cast<std::size_t>(high)],
                 values[static_cast<std::size_t>(low)])) {
            std::swap(values[static_cast<std::size_t>(high)],
                      values[static_cast<std::size_t>(low)]);
        }
        if (less(values[static_cast<std::size_t>(high)],
                 values[static_cast<std::size_t>(mid)])) {
            std::swap(values[static_cast<std::size_t>(high)],
                      values[static_cast<std::size_t>(mid)]);
        }
        const T pivot = values[static_cast<std::size_t>(mid)];
        int i = low;
        int j = high;
        while (i <= j) {
            while (less(values[static_cast<std::size_t>(i)], pivot)) {
                ++i;
            }
            while (less(pivot, values[static_cast<std::size_t>(j)])) {
                --j;
            }
            if (i <= j) {
                std::swap(values[static_cast<std::size_t>(i)],
                          values[static_cast<std::size_t>(j)]);
                ++i;
                --j;
            }
        }
        if (j - low < high - i) {
            quickSortRange(values, low, j, less);
            low = i;
        } else {
            quickSortRange(values, i, high, less);
            high = j;
        }
    }
}

template <class T, class Less>
void quickSort(DynamicArray<T>& values, Less less) {
    if (values.size() < 2) {
        return;
    }
    quickSortRange(values, 0, static_cast<int>(values.size()) - 1, less);
}

}  // namespace uc
