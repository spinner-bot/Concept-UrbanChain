#pragma once

#include "core/map.h"
#include "plan/plan_types.h"

namespace uc {
namespace plan {

// K-means 聚类 + 双距离权重函数。
// 初始化：固定种子的随机不重复站点；空簇会把中心移到当前最远站点。
WeightResult computeWeights(const Map& map, const WeightParams& params);

}  // namespace plan
}  // namespace uc
