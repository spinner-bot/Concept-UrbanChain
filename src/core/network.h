#pragma once

#include <cstdint>
#include <string>

#include "core/line.h"
#include "core/map.h"
#include "ds/dynamic_array.h"
#include "ds/hash_map.h"

namespace uc {

constexpr double kDefaultTransferSeconds = 120.0;

// 线网：地图 + 线路集合 + 换乘参数。
class Network {
public:
    Map& map() { return map_; }
    const Map& map() const { return map_; }

    int addLine(Line line);

    const DynamicArray<Line>& lines() const { return lines_; }
    DynamicArray<Line>& lines() { return lines_; }
    const Line* line(int index) const;

    // 按线路保存的有向换乘走行时间：在 station 从 fromLine 换到 toLine。
    bool setTransferTime(Index station, int fromLine, int toLine, double seconds);
    double transferTime(Index station, int fromLine, int toLine) const;

    bool validate(std::string* error) const;

private:
    static std::uint64_t transferKey(Index station, int fromLine, int toLine);

    Map map_;
    DynamicArray<Line> lines_;
    HashMap<std::uint64_t, double, IntHash> transfers_;
};

}  // namespace uc
