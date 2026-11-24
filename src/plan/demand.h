#pragma once

#include "core/map.h"
#include "plan/plan_types.h"

namespace uc {
namespace plan {

// 重力模型 OD：OD_ij ∝ w_i w_j / d_ij^beta；K>1 时按约定强制 beta=1。
DemandResult computeDemand(const Map& map, const WeightResult& weights,
                           const DemandParams& params);

}  // namespace plan
}  // namespace uc
