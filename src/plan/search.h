#pragma once

#include "core/map.h"
#include "core/network.h"
#include "plan/plan_types.h"

namespace uc {
namespace plan {

struct ScoreParams {
    // 仅用于搜索接受/排序，不是最终总体 R。
    double wCoverage = 0.30;
    double wCost = 0.20;
    double wTime = 0.25;
    double wTransfer = 0.10;
    double wDetour = 0.05;
    double wShort = 0.10;
    double refCost = 1.0;
    double refTime = 1.0;
    double refTransfer = 3.0;
    double refDetour = 3.0;
    double refShort = 1.0;
    // 软覆盖率下限：低于阈值时按平方缺口惩罚，仍保留效率项。
    double minCoverage = 0.8;
    double coveragePenalty = 1.0;
    // 线路相似度惩罚：共享区间（共线）与站点过近都会推高惩罚，
    // 用于避免多条线路堆在市中心同一条走廊。
    double wLineOverlap = 0.10;
    double wLineProximity = 0.03;
    double lineProximityDistance = 7.0;
    // 线路长度充足度：密集城区要求更多站，稀疏郊区/快线允许较短。
    double wLineAdequacy = 0.20;
    int lineAdequacyBaseStops = 7;
    int lineAdequacyMinStops = 3;
    int lineAdequacyMaxStops = 12;
    // 线路形态：净位移 / 实际长度不足时惩罚“来回扭动”，不惩罚单次 90° 弯。
    double wLineWiggle = 0.15;
    double lineStraightnessTarget = 0.65;
};

struct SearchParams {
    int maxLines = 8;
    int topStations = 10;
    int extraAnchors = 12;
    int candidateLimit = 80;
    // 走廊候选：从背景道路走廊生成贯通线路，替代部分任意站点对最短路。
    bool useCorridorCandidates = true;
    bool corridorOnly = false;
    double corridorTurnDeg = 100.0;
    int corridorCandidateLimit = 240;
    // 候选线路与搜索过程中的最短站数；过短的“线路”通常只是接驳段。
    int minLineStations = 2;
    double minCoverageGain = 1e-9;
    bool enableLocalSearch = true;
    int localSearchBudget = 120;
    // 局部移动先做静态指标预筛，再对候选做完整 OD 评价。
    bool prefilter = true;
    double prefilterMargin = 0.0;
    // 搜索结束后做线路级清理：重复线、可贯通线、冗余短线。
    bool enableCleanup = true;
    double maxMergeTurnDeg = 45.0;
    int cleanupBudget = 120;
    ScoreParams score;
};

struct PlanResult {
    DynamicArray<Line> lines;
    Metrics metrics;
    Metrics baseMetrics;
    int candidatesEvaluated = 0;
    int steps = 0;
    int localMoves = 0;
    int mergedLines = 0;
    int duplicateLinesRemoved = 0;
    int redundantLinesRemoved = 0;
    double score = 0.0;
};

// 线路相似度惩罚（共线 + 过近），供测试与实验观察。
double lineSimilarityPenalty(const Map& map, const DynamicArray<Line>& lines,
                             const ScoreParams& score);
// 密度感知的线路长度充足度惩罚：密集城区短线路罚得更重。
double lineAdequacyPenalty(const Map& map, const DynamicArray<Line>& lines,
                           const ScoreParams& score);
// 线路直线度不足惩罚：净位移 / 实际长度越小，惩罚越强。
double lineWigglePenalty(const Map& map, const DynamicArray<Line>& lines,
                         const ScoreParams& score);

// 第一版搜索原型：候选路径池 + 连通性保持的贪心覆盖。
// 评分只使用静态指标（R1 增量 / R2 增量），R3/R4 在最终方案上完整计算。
PlanResult buildGreedyPlan(const Map& map, const WeightResult& weights,
                           const DemandResult& demand, const EvalParams& params,
                           const SearchParams& search);

}  // namespace plan
}  // namespace uc
