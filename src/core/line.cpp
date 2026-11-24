#include "core/line.h"

#include <utility>

#include "core/map.h"

namespace uc {

Line::Line(int id, std::string name, std::uint32_t color)
    : id_(id), name_(static_cast<std::string&&>(name)), color_(color) {}

int LineOps::headwayAt(int timeSec) const {
    for (std::size_t i = 0; i < headway.size(); ++i) {
        const HeadwayPeriod& period = headway[i];
        if (timeSec >= period.startSec && timeSec < period.endSec) {
            return period.headwaySec;
        }
    }
    return 0;
}

bool Line::containsStation(Index station) const {
    for (std::size_t i = 0; i < stations_.size(); ++i) {
        if (stations_[i] == station) {
            return true;
        }
    }
    return false;
}

bool Line::validate(const Map& map, std::string* error) const {
    if (stationCount() < kMinStationsPerLine) {
        if (error != nullptr) {
            *error = "line has fewer than 2 stations";
        }
        return false;
    }

    for (int i = 0; i < stationCount(); ++i) {
        const Index station = stations_[static_cast<std::size_t>(i)];
        if (station < 0 || station >= map.stationCount()) {
            if (error != nullptr) {
                *error = "line references an invalid station index";
            }
            return false;
        }
        for (int j = 0; j < i; ++j) {
            if (stations_[static_cast<std::size_t>(j)] == station) {
                if (error != nullptr) {
                    *error = "line repeats a station";
                }
                return false;
            }
        }
    }

    for (int i = 0; i + 1 < stationCount(); ++i) {
        if (!map.hasEdge(stations_[static_cast<std::size_t>(i)],
                         stations_[static_cast<std::size_t>(i + 1)])) {
            if (error != nullptr) {
                *error = "consecutive line stations have no map edge";
            }
            return false;
        }
    }

    const std::size_t expectedSegments = static_cast<std::size_t>(stationCount() - 1);
    if (ops_.segmentRunSec.size() != expectedSegments) {
        if (error != nullptr) {
            *error = "segmentRunSec size does not match station count - 1";
        }
        return false;
    }
    for (std::size_t i = 0; i < ops_.segmentRunSec.size(); ++i) {
        if (ops_.segmentRunSec[i] < 0.0) {
            if (error != nullptr) {
                *error = "segment run time is negative";
            }
            return false;
        }
    }

    if (ops_.dwellSec < 0.0) {
        if (error != nullptr) {
            *error = "dwell time is negative";
        }
        return false;
    }

    if (ops_.headway.empty()) {
        if (error != nullptr) {
            *error = "line has no headway periods";
        }
        return false;
    }
    int previousEnd = -1;
    for (std::size_t i = 0; i < ops_.headway.size(); ++i) {
        const HeadwayPeriod& period = ops_.headway[i];
        if (period.headwaySec <= 0 || period.startSec >= period.endSec) {
            if (error != nullptr) {
                *error = "invalid headway period";
            }
            return false;
        }
        if (period.startSec < previousEnd) {
            if (error != nullptr) {
                *error = "headway periods overlap or are not sorted";
            }
            return false;
        }
        previousEnd = period.endSec;
    }
    return true;
}

double Line::length(const Map& map) const {
    double total = 0.0;
    for (int i = 0; i + 1 < stationCount(); ++i) {
        total += map.edgeLength(stations_[static_cast<std::size_t>(i)],
                                stations_[static_cast<std::size_t>(i + 1)]);
    }
    return total;
}

}  // namespace uc
