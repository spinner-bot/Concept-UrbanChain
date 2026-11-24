#include "core/network.h"

#include <utility>

#include "ds/sort.h"

namespace uc {

int Network::addLine(Line line) {
    const int index = static_cast<int>(lines_.size());
    line.setId(index);
    lines_.push_back(static_cast<Line&&>(line));
    return index;
}

bool Network::removeLine(int index) {
    if (index < 0 || index >= static_cast<int>(lines_.size())) {
        return false;
    }
    lines_.erase(static_cast<std::size_t>(index));
    for (std::size_t i = static_cast<std::size_t>(index); i < lines_.size(); ++i) {
        lines_[i].setId(static_cast<int>(i));
    }

    const DynamicArray<TransferRecord> records = transferRecords();
    transfers_.clear();
    for (std::size_t i = 0; i < records.size(); ++i) {
        const TransferRecord& record = records[i];
        if (record.fromLine == index || record.toLine == index) {
            continue;
        }
        const int fromLine = record.fromLine > index ? record.fromLine - 1 : record.fromLine;
        const int toLine = record.toLine > index ? record.toLine - 1 : record.toLine;
        transfers_.set(transferKey(record.station, fromLine, toLine), record.seconds);
    }
    return true;
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

DynamicArray<TransferRecord> Network::transferRecords() const {
    DynamicArray<TransferRecord> records;
    records.reserve(transfers_.size());
    transfers_.forEach([&records](std::uint64_t key, double seconds) {
        TransferRecord record;
        record.station = static_cast<Index>((key >> 42) & 0x1fffffull);
        record.fromLine = static_cast<int>((key >> 21) & 0x1fffffull);
        record.toLine = static_cast<int>(key & 0x1fffffull);
        record.seconds = seconds;
        records.push_back(record);
    });
    quickSort(records, [](const TransferRecord& a, const TransferRecord& b) {
        if (a.station != b.station) {
            return a.station < b.station;
        }
        if (a.fromLine != b.fromLine) {
            return a.fromLine < b.fromLine;
        }
        return a.toLine < b.toLine;
    });
    return records;
}

void Network::clearLines() {
    lines_.clear();
    transfers_.clear();
}

bool Network::validate(std::string* error) const {
    // v3 编辑过程中允许暂时存在孤立站点；连通性由 Map::isConnected 单独检查，
    // 导航对不可达站点返回“无路径”。
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
