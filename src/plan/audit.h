#pragma once

#include "core/line.h"
#include "core/map.h"
#include "ds/dynamic_array.h"

namespace uc {
namespace plan {

// 线路结构审计：独立于 R1..R6，用来发现“指标好看但线网畸形”的情况。
// 典型反例：网格图中横向线铺满、没有纵向线；同向断线；重复短线。
enum class LineAxis {
    EastWest = 0,
    NorthSouth = 1,
    Diagonal = 2,
};

struct MapGeometryIssue {
    Index from = kInvalidIndex;
    Index to = kInvalidIndex;
    double geometricLength = 0.0;
    double storedLength = 0.0;
    double ratio = 1.0;
};

struct MapGeometryReport {
    int edgeCount = 0;
    int mismatchedEdges = 0;
    double worstRatio = 1.0;
    // 单位距离建设成本分布；普通路段应接近 1，过河/过山等可明显高于 1。
    double unitCostMin = 0.0;
    double unitCostMax = 0.0;
    double unitCostMean = 0.0;
    int unitCostOutliers = 0;  // 超出 [0.8, 1.25] 的边数，仅作提示
    DynamicArray<MapGeometryIssue> issues;
};

// 检查地图边的几何长度与站点坐标是否一致，并汇总单位距离建设成本。
// length 应等于几何长度；unitCost 可以有意识地偏离 1，用来表达建设难度。
MapGeometryReport auditMapGeometry(const Map& map, double tolerance = 0.05);
void printMapGeometry(const Map& map, const MapGeometryReport& report);

struct NearestEdgeReport {
    int required = 0;       // X = 1 + stationCount / 10
    int checkedStations = 0;
    int violatedStations = 0;  // 最近 X 个站点中没有全部连边
    int missingEdges = 0;
};

// 强制连通规则：每个站点与最近的 X 个站点必须有边。
NearestEdgeReport auditNearestEdges(const Map& map);
void printNearestEdges(const NearestEdgeReport& report);

struct LineMergeOpportunity {
    int firstLine = -1;
    int secondLine = -1;
    Index junction = kInvalidIndex;
    double turnDeg = 180.0;  // 0 表示完全贯通，180 表示原路折返
    bool simple = false;     // 合并后仍是无重复站点的简单路径
};

struct LineStructure {
    int stationCount = 0;
    int lineCount = 0;

    int shortLines = 0;
    int shortStationThreshold = 3;
    int duplicateExtraLines = 0;  // 同一站点集合的多余线路数

    int axisLines[3] = {0, 0, 0};
    int axisEdges[3] = {0, 0, 0};
    int uniqueEdges = 0;
    double axisLineEntropy = 0.0;  // 0=单方向，1=三方向均衡
    double axisEdgeEntropy = 0.0;

    int maxLinesAtStation = 0;
    int stationsUncovered = 0;
    int stationsWithOneLine = 0;
    int stationsWithMultipleLines = 0;
    int lineCountHistogram[5] = {0, 0, 0, 0, 0};  // 0,1,2,3,4+

    // 线路相似度：共享区间比例与站点近邻比例，用于检查线路是否堆在一起。
    double averageEdgeOverlap = 0.0;
    double maxEdgeOverlap = 0.0;
    double averageStationProximity = 0.0;
    double maxStationProximity = 0.0;
    int clusteredPairs = 0;  // 共享区间或近邻比例 >= 0.5 的线路对

    int sharedEndpointPairs = 0;
    int simpleMergePairs = 0;
    int straightMergePairs = 0;  // 合并转角 <= 45 度，属于同向贯通
    int reversalPairs = 0;       // 合并转角 >= 135 度，属于原路折返
    DynamicArray<LineMergeOpportunity> mergeOpportunities;
};

LineStructure auditLines(const Map& map, const DynamicArray<Line>& lines,
                         int shortStationThreshold = 3);

// 在两条线路的公共端点处尝试贯通；选择转角最小的合法简单路径。
// 合并后的线路保留 first 的编号、名称和颜色，运营参数按统一默认值重建。
bool mergeLinePair(const Map& map, const Line& first, const Line& second,
                   Line& merged, double* turnDeg = nullptr);

const char* axisName(LineAxis axis);
void printLineStructure(const LineStructure& structure);

}  // namespace plan
}  // namespace uc
