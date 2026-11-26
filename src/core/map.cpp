#include "core/map.h"

#include <cmath>

#include "ds/union_find.h"

namespace uc {

bool Map::addStation(const Station& station, Index* outIndex) {
    if (station.id.empty() || findStation(station.id) != kInvalidIndex) {
        return false;
    }
    const Index index = static_cast<Index>(stations_.size());
    stations_.push_back(station);
    stationIndex_.insert(station.id, index);
    adjacency_.push_back(DynamicArray<AdjEdge>());
    resizeMatrix(index, index + 1);
    if (outIndex != nullptr) {
        *outIndex = index;
    }
    return true;
}

bool Map::setStationPosition(Index index, const Vec2& position) {
    if (index < 0 || index >= stationCount()) {
        return false;
    }
    stations_[static_cast<std::size_t>(index)].position = position;
    return true;
}

bool Map::setStationName(Index index, const std::string& name) {
    if (index < 0 || index >= stationCount()) {
        return false;
    }
    stations_[static_cast<std::size_t>(index)].name = name;
    return true;
}

Index Map::findStation(const std::string& id) const {
    const Index* found = stationIndex_.find(id);
    return found == nullptr ? kInvalidIndex : *found;
}

bool Map::addEdge(Index u, Index v, double unitCost, double length) {
    const Index count = stationCount();
    if (u < 0 || v < 0 || u >= count || v >= count || u == v) {
        return false;
    }
    if (unitCost < 0.0 || hasEdge(u, v)) {
        return false;
    }
    double actualLength = length;
    if (actualLength < 0.0) {
        actualLength = euclideanDistance(stations_[static_cast<std::size_t>(u)].position,
                                         stations_[static_cast<std::size_t>(v)].position);
    }
    if (actualLength < 1e-9) {
        // 两端重合时无法给出有意义的长度，退回单位长度，避免零长度边。
        actualLength = 1.0;
    }
    const Index edgeId = static_cast<Index>(edges_.size());
    edges_.push_back(EdgeRecord{u, v, unitCost, actualLength});
    adjacency_[static_cast<std::size_t>(u)].push_back(AdjEdge{v, unitCost, actualLength, edgeId});
    adjacency_[static_cast<std::size_t>(v)].push_back(AdjEdge{u, unitCost, actualLength, edgeId});

    const std::size_t offsetUV = static_cast<std::size_t>(u) * count + static_cast<std::size_t>(v);
    const std::size_t offsetVU = static_cast<std::size_t>(v) * count + static_cast<std::size_t>(u);
    matrixUnitCost_[offsetUV] = unitCost;
    matrixUnitCost_[offsetVU] = unitCost;
    matrixLength_[offsetUV] = actualLength;
    matrixLength_[offsetVU] = actualLength;
    return true;
}

bool Map::removeEdge(Index u, Index v) {
    if (!hasEdge(u, v)) {
        return false;
    }
    Index found = kInvalidIndex;
    for (std::size_t i = 0; i < edges_.size(); ++i) {
        const EdgeRecord& edge = edges_[i];
        if ((edge.from == u && edge.to == v) || (edge.from == v && edge.to == u)) {
            found = static_cast<Index>(i);
            break;
        }
    }
    if (found == kInvalidIndex) {
        return false;
    }
    edges_.erase(static_cast<std::size_t>(found));

    const Index count = stationCount();
    for (Index i = 0; i < count; ++i) {
        adjacency_[static_cast<std::size_t>(i)].clear();
    }
    for (std::size_t i = 0; i < matrixUnitCost_.size(); ++i) {
        matrixUnitCost_[i] = kInfinity;
        matrixLength_[i] = 0.0;
    }
    for (std::size_t i = 0; i < edges_.size(); ++i) {
        const EdgeRecord& edge = edges_[i];
        const Index edgeId = static_cast<Index>(i);
        adjacency_[static_cast<std::size_t>(edge.from)].push_back(
            AdjEdge{edge.to, edge.unitCost, edge.length, edgeId});
        adjacency_[static_cast<std::size_t>(edge.to)].push_back(
            AdjEdge{edge.from, edge.unitCost, edge.length, edgeId});
        const std::size_t offset =
            static_cast<std::size_t>(edge.from) * count + static_cast<std::size_t>(edge.to);
        const std::size_t reverse =
            static_cast<std::size_t>(edge.to) * count + static_cast<std::size_t>(edge.from);
        matrixUnitCost_[offset] = edge.unitCost;
        matrixUnitCost_[reverse] = edge.unitCost;
        matrixLength_[offset] = edge.length;
        matrixLength_[reverse] = edge.length;
    }
    return true;
}

bool Map::setEdgeWeight(Index u, Index v, double cost, double length) {
    if (!hasEdge(u, v) || cost < 0.0 || length < 0.0) {
        return false;
    }
    for (std::size_t i = 0; i < edges_.size(); ++i) {
        EdgeRecord& edge = edges_[i];
        const bool forward = edge.from == u && edge.to == v;
        const bool backward = edge.from == v && edge.to == u;
        if (!forward && !backward) {
            continue;
        }
        edge.unitCost = cost;
        edge.length = length;
        for (std::size_t j = 0; j < adjacency_[static_cast<std::size_t>(u)].size(); ++j) {
            AdjEdge& candidate = adjacency_[static_cast<std::size_t>(u)][j];
            if (candidate.to == v && candidate.edgeId == static_cast<Index>(i)) {
                candidate.unitCost = cost;
                candidate.length = length;
            }
        }
        for (std::size_t j = 0; j < adjacency_[static_cast<std::size_t>(v)].size(); ++j) {
            AdjEdge& candidate = adjacency_[static_cast<std::size_t>(v)][j];
            if (candidate.to == u && candidate.edgeId == static_cast<Index>(i)) {
                candidate.unitCost = cost;
                candidate.length = length;
            }
        }
        const Index count = stationCount();
        const std::size_t offset =
            static_cast<std::size_t>(u) * count + static_cast<std::size_t>(v);
        const std::size_t reverse =
            static_cast<std::size_t>(v) * count + static_cast<std::size_t>(u);
        matrixUnitCost_[offset] = cost;
        matrixUnitCost_[reverse] = cost;
        matrixLength_[offset] = length;
        matrixLength_[reverse] = length;
        return true;
    }
    return false;
}

bool Map::hasEdge(Index u, Index v) const {
    const Index count = stationCount();
    if (u < 0 || v < 0 || u >= count || v >= count || u == v) {
        return false;
    }
    return matrixUnitCost_[static_cast<std::size_t>(u) * count + static_cast<std::size_t>(v)] != kInfinity;
}

double Map::edgeUnitCost(Index u, Index v) const {
    if (u == v) {
        return 0.0;
    }
    const Index count = stationCount();
    if (u < 0 || v < 0 || u >= count || v >= count) {
        return kInfinity;
    }
    return matrixUnitCost_[static_cast<std::size_t>(u) * count + static_cast<std::size_t>(v)];
}

double Map::edgeLength(Index u, Index v) const {
    if (u == v) {
        return 0.0;
    }
    const Index count = stationCount();
    if (u < 0 || v < 0 || u >= count || v >= count) {
        return kInfinity;
    }
    return matrixLength_[static_cast<std::size_t>(u) * count + static_cast<std::size_t>(v)];
}

double Map::edgeBuildCost(Index u, Index v) const {
    const double unit = edgeUnitCost(u, v);
    const double length = edgeLength(u, v);
    if (std::isinf(unit) || std::isinf(length)) {
        return kInfinity;
    }
    return unit * length;
}

bool Map::isConnected() const {
    const Index count = stationCount();
    if (count <= 1) {
        return true;
    }
    UnionFind sets(count);
    for (std::size_t i = 0; i < edges_.size(); ++i) {
        sets.unite(edges_[i].from, edges_[i].to);
    }
    return sets.count() == 1;
}

void Map::addBackgroundFeature(BackgroundFeature feature) {
    background_.push_back(static_cast<BackgroundFeature&&>(feature));
}

void Map::clear() {
    stations_.clear();
    stationIndex_.clear();
    adjacency_.clear();
    edges_.clear();
    matrixUnitCost_.clear();
    matrixLength_.clear();
    background_.clear();
}

void Map::resizeMatrix(Index oldCount, Index newCount) {
    const std::size_t newSize = static_cast<std::size_t>(newCount) * static_cast<std::size_t>(newCount);
    DynamicArray<double> newCost;
    DynamicArray<double> newLength;
    newCost.resize(newSize);
    newLength.resize(newSize);
    for (std::size_t i = 0; i < newSize; ++i) {
        newCost[i] = kInfinity;
        newLength[i] = 0.0;
    }
    for (Index row = 0; row < oldCount; ++row) {
        for (Index column = 0; column < oldCount; ++column) {
            const std::size_t oldOffset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(oldCount) +
                static_cast<std::size_t>(column);
            const std::size_t newOffset =
                static_cast<std::size_t>(row) * static_cast<std::size_t>(newCount) +
                static_cast<std::size_t>(column);
            newCost[newOffset] = matrixUnitCost_[oldOffset];
            newLength[newOffset] = matrixLength_[oldOffset];
        }
    }
    matrixUnitCost_ = static_cast<DynamicArray<double>&&>(newCost);
    matrixLength_ = static_cast<DynamicArray<double>&&>(newLength);
}

}  // namespace uc
