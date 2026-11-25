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
