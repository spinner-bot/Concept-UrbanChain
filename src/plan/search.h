#pragma once

#include "core/map.h"
#include "core/network.h"
#include "plan/plan_types.h"

namespace uc {
namespace plan {

struct SearchParams {
    int maxLines = 8;
    int topStations = 10;
    int candidateLimit = 80;
    double minCoverageGain = 1e-9;
};

struct PlanResult {
    DynamicArray<Line> lines;
    Metrics metrics;
    int candidatesEvaluated = 0;
    int steps = 0;
};

// 第一版搜索原型：候选路径池 + 连通性保持的贪心覆盖。
// 评分只使用静态指标（R1 增量 / R2 增量），R3/R4 在最终方案上完整计算。
PlanResult buildGreedyPlan(const Map& map, const WeightResult& weights,
                           const DemandResult& demand, const EvalParams& params,
                           const SearchParams& search);

}  // namespace plan
}  // namespace uc
