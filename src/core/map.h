#pragma once

#include <string>

#include "core/background.h"
#include "core/station.h"
#include "core/types.h"
#include "ds/dynamic_array.h"
#include "ds/hash_map.h"

namespace uc {

struct EdgeRecord {
    Index from = kInvalidIndex;
    Index to = kInvalidIndex;
    double cost = 0.0;    // 建设成本，由地图数据给出
    double length = 0.0;  // 几何长度；缺省时等于 cost
};

struct AdjEdge {
    Index to = kInvalidIndex;
    double cost = 0.0;
    double length = 0.0;
    Index edgeId = kInvalidIndex;
};

// 地图：站点集合 + 无向带权图 + 背景层占位。
// 同时维护邻接矩阵（快速判断边是否存在）、邻接表（遍历）和边表（统计）。
class Map {
public:
    Map() = default;

    bool addStation(const Station& station, Index* outIndex = nullptr);
    Index findStation(const std::string& id) const;

    Index stationCount() const { return static_cast<Index>(stations_.size()); }
    const Station& station(Index index) const { return stations_.at(static_cast<std::size_t>(index)); }
    const DynamicArray<Station>& stations() const { return stations_; }

    bool setStationPosition(Index index, const Vec2& position);
    bool setStationName(Index index, const std::string& name);

    bool addEdge(Index u, Index v, double cost, double length = -1.0);
    bool removeEdge(Index u, Index v);
    bool setEdgeWeight(Index u, Index v, double cost, double length);
    bool hasEdge(Index u, Index v) const;
    double edgeCost(Index u, Index v) const;
    double edgeLength(Index u, Index v) const;

    const DynamicArray<AdjEdge>& neighbors(Index station) const {
        return adjacency_.at(static_cast<std::size_t>(station));
    }
    const DynamicArray<EdgeRecord>& edges() const { return edges_; }

    bool isConnected() const;

    void addBackgroundFeature(BackgroundFeature feature);
    const DynamicArray<BackgroundFeature>& backgroundFeatures() const {
        return background_;
    }
    DynamicArray<BackgroundFeature>& backgroundFeatures() { return background_; }
    void clearBackground() { background_.clear(); }

    void clear();

private:
    void resizeMatrix(Index oldCount, Index newCount);

    DynamicArray<Station> stations_;
    HashMap<std::string, Index, Fnv1aHash> stationIndex_;
    DynamicArray<DynamicArray<AdjEdge>> adjacency_;
    DynamicArray<EdgeRecord> edges_;
    DynamicArray<double> matrixCost_;
    DynamicArray<double> matrixLength_;
    DynamicArray<BackgroundFeature> background_;
};

}  // namespace uc
