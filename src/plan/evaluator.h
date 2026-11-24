#pragma once

#include "core/network.h"
#include "plan/plan_types.h"

namespace uc {
namespace plan {

// 评价一个已经构造好的线网（保留其运营参数与换乘记录）。
Metrics evaluateNetwork(const Network& network, const WeightResult& weights,
                        const DemandResult& demand, const EvalParams& params);

// 用默认运营参数构造线网后评价；用于规划候选方案。
Metrics evaluate(const Map& map, const DynamicArray<Line>& lines,
                 const WeightResult& weights, const DemandResult& demand,
                 const EvalParams& params);

}  // namespace plan
}  // namespace uc
