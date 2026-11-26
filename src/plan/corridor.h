#pragma once

#include "core/line.h"
#include "core/map.h"
#include "ds/dynamic_array.h"
#include "plan/plan_types.h"

namespace uc {
namespace plan {

// 走廊骨架：把道路边收缩成“路口—路口”的走廊段，供线路级生成使用。
struct CorridorSegment {
    int id = -1;
    DynamicArray<Index> stations;  // 走廊经过的站点顺序
    double length = 0.0;           // 实际长度
    double roadWidth = 0.0;        // 沿途背景道路宽度的最大值
    int mapEdgeCount = 0;
};

struct CorridorGraph {
    DynamicArray<CorridorSegment> segments;
    // 与 map.edges() 对齐：
    // edgeIsMainCorridor：属于主/次干道走廊，用于生成候选线路；
    // edgeIsRoad：任意背景道路（含局部街巷），线路中段可以使用；
    // 两者都为 0 的是非道路连接边（kNN 等），只允许线路端点接入。
    DynamicArray<unsigned char> edgeIsMainCorridor;
    DynamicArray<unsigned char> edgeIsRoad;
    int mainCorridorEdgeCount = 0;
    int roadEdgeCount = 0;
    int connectorEdgeCount = 0;
    int junctionCount = 0;  // 走廊共享站点的数量
    int maxSegmentStations = 0;
    double totalRoadLength = 0.0;
};

struct CorridorOptions {
    // 站点图上按 OD 流量选走廊边：取流量最高的这一比例。
    double corridorEdgeRatio = 0.25;
};

// 站点/需求驱动的走廊骨架：只用站点分布与 OD 需求，不使用背景道路。
// 方法：在站点图上为每个 OD 分配流量，取高流量边，收缩成走廊段。
CorridorGraph buildStationCorridorGraph(const Map& map, const WeightResult& weights,
                                        const DemandResult& demand,
                                        const CorridorOptions& options = {});


void printCorridorGraph(const CorridorGraph& graph);

struct CorridorLineUsage {
    int lineEdges = 0;
    int mainCorridorEdges = 0;
    int roadEdges = 0;      // 任意道路（含局部街巷）
    int connectorEdges = 0; // 非道路连接边
};
CorridorLineUsage corridorLineUsage(const Map& map, const CorridorGraph& graph,
                                   const DynamicArray<Line>& lines);
void printCorridorLineUsage(const CorridorLineUsage& usage);

// 从走廊骨架生成“沿走廊贯通”的候选线路：
// - 每条走廊本身；
// - 两条走廊在共享站贯通（转角不超过 maxTurnDeviationDeg）。
DynamicArray<DynamicArray<Index>> buildCorridorLineCandidates(
    const Map& map, const CorridorGraph& graph,
    double maxTurnDeviationDeg = 100.0, int limit = 400);

}  // namespace plan
}  // namespace uc
