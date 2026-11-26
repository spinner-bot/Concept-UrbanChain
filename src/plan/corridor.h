#pragma once

#include "core/line.h"
#include "core/map.h"
#include "ds/dynamic_array.h"

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
    // 站点到背景道路折线的距离阈值（地图单位）。
    double roadTolerance = 2.0;
    // 只有宽度达到该值的道路才作为主走廊；局部街巷仍算道路，但不生成走廊候选。
    double minCorridorWidth = 1.5;
};

// 提取走廊骨架。道路边来自地图背景中的 road 要素；
// 不在道路上的边（例如 kNN 强制连边）标记为连接边，不进入走廊。
CorridorGraph buildCorridorGraph(const Map& map, const CorridorOptions& options = {});

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
