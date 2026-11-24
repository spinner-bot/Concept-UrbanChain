#pragma once

#include "core/types.h"
#include "ds/dynamic_array.h"

namespace uc {

// 向心 Catmull-Rom 样条（alpha=0.5），曲线精确经过所有输入关键点。
// 用于线路渲染；不参与导航与规划计算。
DynamicArray<Vec2> catmullRomSpline(const DynamicArray<Vec2>& points,
                                    int samplesPerSegment = 24,
                                    double alpha = 0.5);

}  // namespace uc
