#pragma once

#include <cstdint>
#include <string>

#include "core/types.h"
#include "ds/dynamic_array.h"

namespace uc {

class Map;

struct HeadwayPeriod {
    int startSec = 0;    // 含
    int endSec = 0;      // 不含
    int headwaySec = 0;  // 发车间隔
};

// 线路运营参数。v3 使用显式区段运行时间与分时段发车间隔；
// 后续版本可在不改变 Line 的前提下扩展为连续发车间隔函数。
struct LineOps {
    DynamicArray<double> segmentRunSec;  // 长度 = 站点数 - 1
    double dwellSec = 30.0;              // 中间站停站时间
    DynamicArray<HeadwayPeriod> headway; // 按时间升序、互不重叠

    // 返回时刻 timeSec 所在时段的发车间隔；无可用时段返回 0。
    int headwayAt(int timeSec) const;
};

// 线路：地图上的一条合法简单路径 + 运营参数。
class Line {
public:
    Line() = default;
    Line(int id, std::string name, std::uint32_t color);

    int id() const { return id_; }
    void setId(int id) { id_ = id; }

    const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = static_cast<std::string&&>(name); }

    std::uint32_t color() const { return color_; }
    void setColor(std::uint32_t color) { color_ = color; }

    const DynamicArray<Index>& stations() const { return stations_; }
    DynamicArray<Index>& stations() { return stations_; }
    void setStations(DynamicArray<Index> stations) {
        stations_ = static_cast<DynamicArray<Index>&&>(stations);
    }

    int stationCount() const { return static_cast<int>(stations_.size()); }
    bool containsStation(Index station) const;

    const LineOps& ops() const { return ops_; }
    LineOps& ops() { return ops_; }

    // v3 默认值：线路至少 2 个站点。该约束在 v4 实验中可调整。
    static constexpr int kMinStationsPerLine = 2;

    bool validate(const Map& map, std::string* error) const;
    double length(const Map& map) const;

private:
    int id_ = -1;
    std::string name_;
    std::uint32_t color_ = 0;
    DynamicArray<Index> stations_;
    LineOps ops_;
};

}  // namespace uc
