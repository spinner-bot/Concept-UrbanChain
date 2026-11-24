#include "core/map.h"

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

bool Map::addEdge(Index u, Index v, double cost, double length) {
    const Index count = stationCount();
    if (u < 0 || v < 0 || u >= count || v >= count || u == v) {
        return false;
    }
    if (cost < 0.0 || hasEdge(u, v)) {
        return false;
    }
    const double actualLength = length < 0.0 ? cost : length;
    const Index edgeId = static_cast<Index>(edges_.size());
    edges_.push_back(EdgeRecord{u, v, cost, actualLength});
    adjacency_[static_cast<std::size_t>(u)].push_back(AdjEdge{v, cost, actualLength, edgeId});
    adjacency_[static_cast<std::size_t>(v)].push_back(AdjEdge{u, cost, actualLength, edgeId});

    const std::size_t offsetUV = static_cast<std::size_t>(u) * count + static_cast<std::size_t>(v);
    const std::size_t offsetVU = static_cast<std::size_t>(v) * count + static_cast<std::size_t>(u);
    matrixCost_[offsetUV] = cost;
    matrixCost_[offsetVU] = cost;
    matrixLength_[offsetUV] = actualLength;
    matrixLength_[offsetVU] = actualLength;
    return true;
}

bool Map::hasEdge(Index u, Index v) const {
    const Index count = stationCount();
    if (u < 0 || v < 0 || u >= count || v >= count || u == v) {
        return false;
    }
    return matrixCost_[static_cast<std::size_t>(u) * count + static_cast<std::size_t>(v)] != kInfinity;
}

double Map::edgeCost(Index u, Index v) const {
    if (u == v) {
        return 0.0;
    }
    const Index count = stationCount();
    if (u < 0 || v < 0 || u >= count || v >= count) {
        return kInfinity;
    }
    return matrixCost_[static_cast<std::size_t>(u) * count + static_cast<std::size_t>(v)];
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

void Map::clear() {
    stations_.clear();
    stationIndex_.clear();
    adjacency_.clear();
    edges_.clear();
    matrixCost_.clear();
    matrixLength_.clear();
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
            newCost[newOffset] = matrixCost_[oldOffset];
            newLength[newOffset] = matrixLength_[oldOffset];
        }
    }
    matrixCost_ = static_cast<DynamicArray<double>&&>(newCost);
    matrixLength_ = static_cast<DynamicArray<double>&&>(newLength);
}

}  // namespace uc
