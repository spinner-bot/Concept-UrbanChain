#pragma once

#include "ds/dynamic_array.h"

namespace uc {

// 自写并查集：按秩合并 + 路径压缩。
class UnionFind {
public:
    UnionFind() = default;

    explicit UnionFind(int count) { reset(count); }

    void reset(int count) {
        parent_.resize(static_cast<std::size_t>(count));
        rank_.resize(static_cast<std::size_t>(count));
        count_ = count;
        for (int i = 0; i < count; ++i) {
            parent_[static_cast<std::size_t>(i)] = i;
            rank_[static_cast<std::size_t>(i)] = 0;
        }
    }

    int find(int x) {
        int root = x;
        while (parent_[static_cast<std::size_t>(root)] != root) {
            root = parent_[static_cast<std::size_t>(root)];
        }
        while (parent_[static_cast<std::size_t>(x)] != root) {
            int next = parent_[static_cast<std::size_t>(x)];
            parent_[static_cast<std::size_t>(x)] = root;
            x = next;
        }
        return root;
    }

    bool unite(int a, int b) {
        int rootA = find(a);
        int rootB = find(b);
        if (rootA == rootB) {
            return false;
        }
        if (rank_[static_cast<std::size_t>(rootA)] < rank_[static_cast<std::size_t>(rootB)]) {
            int tmp = rootA;
            rootA = rootB;
            rootB = tmp;
        }
        parent_[static_cast<std::size_t>(rootB)] = rootA;
        if (rank_[static_cast<std::size_t>(rootA)] == rank_[static_cast<std::size_t>(rootB)]) {
            ++rank_[static_cast<std::size_t>(rootA)];
        }
        --count_;
        return true;
    }

    bool connected(int a, int b) { return find(a) == find(b); }

    int count() const { return count_; }

private:
    DynamicArray<int> parent_;
    DynamicArray<int> rank_;
    int count_ = 0;
};

}  // namespace uc
