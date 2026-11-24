#include "core/network.h"

#include <utility>

namespace uc {

int Network::addLine(Line line) {
    const int index = static_cast<int>(lines_.size());
    line.setId(index);
    lines_.push_back(static_cast<Line&&>(line));
    return index;
}

const Line* Network::line(int index) const {
    if (index < 0 || index >= static_cast<int>(lines_.size())) {
        return nullptr;
    }
    return &lines_[static_cast<std::size_t>(index)];
}

bool Network::setTransferTime(Index station, int fromLine, int toLine, double seconds) {
    if (station < 0 || fromLine < 0 || toLine < 0 || fromLine == toLine || seconds < 0.0) {
        return false;
    }
    transfers_.set(transferKey(station, fromLine, toLine), seconds);
    return true;
}

double Network::transferTime(Index station, int fromLine, int toLine) const {
    if (fromLine == toLine) {
        return 0.0;
    }
    const double* found = transfers_.find(transferKey(station, fromLine, toLine));
    return found == nullptr ? kDefaultTransferSeconds : *found;
}

bool Network::validate(std::string* error) const {
    if (map_.stationCount() > 1 && !map_.isConnected()) {
        if (error != nullptr) {
            *error = "map graph is not connected";
        }
        return false;
    }
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        if (!lines_[i].validate(map_, error)) {
            return false;
        }
    }
    return true;
}

std::uint64_t Network::transferKey(Index station, int fromLine, int toLine) {
    // 假设站点与线路数量均小于 2^21，足够课程项目使用。
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(station)) << 42) |
           (static_cast<std::uint64_t>(static_cast<std::uint32_t>(fromLine)) << 21) |
           static_cast<std::uint64_t>(static_cast<std::uint32_t>(toLine));
}

}  // namespace uc
