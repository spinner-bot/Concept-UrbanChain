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
};

struct SearchParams {
    int maxLines = 8;
    int topStations = 10;
    int extraAnchors = 12;
    int candidateLimit = 80;
    double minCoverageGain = 1e-9;
    bool enableLocalSearch = true;
    int localSearchBudget = 120;
    ScoreParams score;
};

struct PlanResult {
    DynamicArray<Line> lines;
    Metrics metrics;
    Metrics baseMetrics;
    int candidatesEvaluated = 0;
    int steps = 0;
    int localMoves = 0;
    double score = 0.0;
};

// 第一版搜索原型：候选路径池 + 连通性保持的贪心覆盖。
// 评分只使用静态指标（R1 增量 / R2 增量），R3/R4 在最终方案上完整计算。
PlanResult buildGreedyPlan(const Map& map, const WeightResult& weights,
                           const DemandResult& demand, const EvalParams& params,
                           const SearchParams& search);

}  // namespace plan
}  // namespace uc
