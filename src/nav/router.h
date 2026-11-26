#pragma once

#include "core/network.h"
#include "ds/dynamic_array.h"

namespace uc {

struct JourneyLeg {
    int line = -1;
    Index fromStation = kInvalidIndex;
    Index toStation = kInvalidIndex;
    double boardTime = 0.0;
    double alightTime = 0.0;
    // 本段在车内的地图距离（沿线路累计区间长度，不含换乘步行）。
    double distance = 0.0;
};

struct Journey {
    bool found = false;
    double departTime = 0.0;
    double arriveTime = 0.0;
    int transfers = 0;
    DynamicArray<JourneyLeg> legs;
};

// 时间相关最短路导航器。
// 状态为 (站点, 当前线路)；等待时间取到达站台时刻对应的发车间隔的一半。
class Router {
public:
    Journey findRoute(const Network& network, Index origin, Index destination,
                      double departTimeSec) const;
};

}  // namespace uc
