#include "edit/editor.h"

#include <cmath>
#include <utility>

#include "io/line_io.h"
#include "io/map_io.h"

namespace uc {
namespace {

bool fail(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

}  // namespace

void Editor::newDocument() {
    network_ = Network();
    undo_.clear();
    redo_.clear();
    dirty_ = false;
}

bool Editor::load(const std::string& mapPath, const std::string& linePath,
                  std::string* error) {
    Network staged;
    if (!io::loadMap(mapPath, staged.map(), error)) {
        return false;
    }
    if (!io::loadLines(linePath, staged, error)) {
        return false;
    }
    network_ = static_cast<Network&&>(staged);
    undo_.clear();
    redo_.clear();
    dirty_ = false;
    return true;
}

bool Editor::save(const std::string& mapPath, const std::string& linePath,
                  std::string* error) {
    std::string validateError;
    if (!network_.validate(&validateError)) {
        return fail(error, "cannot save: " + validateError);
    }
    if (!io::saveMap(mapPath, network_.map(), error)) {
        return false;
    }
    if (!io::saveLines(linePath, network_, error)) {
        return false;
    }
    dirty_ = false;
    return true;
}

bool Editor::validate(std::string* error) const { return network_.validate(error); }

std::string Editor::nextStationId() const {
    for (int i = 1;; ++i) {
        const std::string id = "S" + std::to_string(i);
        if (network_.map().findStation(id) == kInvalidIndex) {
            return id;
        }
    }
}

bool Editor::checkStation(Index station, std::string* error) const {
    if (station < 0 || station >= network_.map().stationCount()) {
        return fail(error, "invalid station index");
    }
    return true;
}

bool Editor::checkLine(int line, std::string* error) const {
    if (line < 0 || line >= static_cast<int>(network_.lines().size())) {
        return fail(error, "invalid line index");
    }
    return true;
}

Index Editor::addStation(const std::string& name, double x, double y, std::string* error) {
    Station station;
    station.id = nextStationId();
    station.name = name.empty() ? station.id : name;
    station.position = Vec2{x, y};

    pushUndo();
    Index index = kInvalidIndex;
    if (!network_.map().addStation(station, &index)) {
        rollbackUndo();
        fail(error, "failed to add station");
        return kInvalidIndex;
    }
    dirty_ = true;
    return index;
}

bool Editor::moveStation(Index station, double x, double y, std::string* error) {
    if (!checkStation(station, error)) {
        return false;
    }
    pushUndo();
    if (!network_.map().setStationPosition(station, Vec2{x, y})) {
        rollbackUndo();
        return fail(error, "failed to move station");
    }
    dirty_ = true;
    return true;
}

bool Editor::renameStation(Index station, const std::string& name, std::string* error) {
    if (!checkStation(station, error)) {
        return false;
    }
    pushUndo();
    if (!network_.map().setStationName(station, name)) {
        rollbackUndo();
        return fail(error, "failed to rename station");
    }
    dirty_ = true;
    return true;
}

bool Editor::removeStation(Index station, std::string* error) {
    if (!checkStation(station, error)) {
        return false;
    }
    for (std::size_t i = 0; i < network_.lines().size(); ++i) {
        if (network_.lines()[i].containsStation(station)) {
            return fail(error, "station is used by line " +
                                   network_.lines()[i].name() +
                                   "; remove it from the line first");
        }
    }

    const Map& oldMap = network_.map();
    const Index oldCount = oldMap.stationCount();
    DynamicArray<Index> remap;
    remap.resize(static_cast<std::size_t>(oldCount));
    for (Index i = 0; i < oldCount; ++i) {
        remap[static_cast<std::size_t>(i)] = kInvalidIndex;
    }

    Map newMap;
    for (Index i = 0; i < oldCount; ++i) {
        if (i == station) {
            continue;
        }
        Index newIndex = kInvalidIndex;
        if (!newMap.addStation(oldMap.station(i), &newIndex)) {
            return fail(error, "failed to rebuild map while removing station");
        }
        remap[static_cast<std::size_t>(i)] = newIndex;
    }
    for (std::size_t i = 0; i < oldMap.edges().size(); ++i) {
        const EdgeRecord& edge = oldMap.edges()[i];
        if (edge.from == station || edge.to == station) {
            continue;
        }
        const Index from = remap[static_cast<std::size_t>(edge.from)];
        const Index to = remap[static_cast<std::size_t>(edge.to)];
        if (from == kInvalidIndex || to == kInvalidIndex ||
            !newMap.addEdge(from, to, edge.unitCost, edge.length)) {
            return fail(error, "failed to rebuild edges while removing station");
        }
    }
    for (std::size_t i = 0; i < oldMap.backgroundFeatures().size(); ++i) {
        newMap.addBackgroundFeature(oldMap.backgroundFeatures()[i]);
    }

    Network staged;
    staged.map() = static_cast<Map&&>(newMap);
    for (std::size_t i = 0; i < network_.lines().size(); ++i) {
        Line line = network_.lines()[i];
        DynamicArray<Index> stops;
        for (std::size_t j = 0; j < line.stations().size(); ++j) {
            const Index remapped = remap[static_cast<std::size_t>(line.stations()[j])];
            if (remapped == kInvalidIndex) {
                return fail(error, "line references the station being removed");
            }
            stops.push_back(remapped);
        }
        line.setStations(static_cast<DynamicArray<Index>&&>(stops));
        staged.addLine(static_cast<Line&&>(line));
    }
    const DynamicArray<TransferRecord> transfers = network_.transferRecords();
    for (std::size_t i = 0; i < transfers.size(); ++i) {
        const TransferRecord& record = transfers[i];
        if (record.station == station) {
            continue;
        }
        const Index remapped = remap[static_cast<std::size_t>(record.station)];
        if (remapped == kInvalidIndex) {
            continue;
        }
        staged.setTransferTime(remapped, record.fromLine, record.toLine, record.seconds);
    }

    std::string validateError;
    if (!staged.validate(&validateError)) {
        return fail(error, "cannot delete station: " + validateError);
    }

    pushUndo();
    network_ = static_cast<Network&&>(staged);
    dirty_ = true;
    return true;
}

bool Editor::addEdge(Index from, Index to, double cost, double length, std::string* error) {
    if (!checkStation(from, error) || !checkStation(to, error)) {
        return false;
    }
    if (from == to) {
        return fail(error, "cannot connect a station to itself");
    }
    if (network_.map().hasEdge(from, to)) {
        return fail(error, "edge already exists");
    }
    pushUndo();
    if (!network_.map().addEdge(from, to, cost, length)) {
        rollbackUndo();
        return fail(error, "failed to add edge");
    }
    dirty_ = true;
    return true;
}

bool Editor::addEdge(Index from, Index to, std::string* error) {
    if (!checkStation(from, error) || !checkStation(to, error)) {
        return false;
    }
    const double distance = euclideanDistance(network_.map().station(from).position,
                                              network_.map().station(to).position);
    // 默认单位距离成本为 1，长度取几何距离。
    return addEdge(from, to, 1.0, distance, error);
}

bool Editor::setEdgeWeight(Index from, Index to, double cost, double length,
                           std::string* error) {
    if (!checkStation(from, error) || !checkStation(to, error)) {
        return false;
    }
    if (!network_.map().hasEdge(from, to)) {
        return fail(error, "edge does not exist");
    }
    if (cost < 0.0 || length < 0.0) {
        return fail(error, "edge unit cost and length cannot be negative");
    }
    pushUndo();
    if (!network_.map().setEdgeWeight(from, to, cost, length)) {
        rollbackUndo();
        return fail(error, "failed to update edge");
    }
    dirty_ = true;
    return true;
}

bool Editor::removeEdge(Index from, Index to, std::string* error) {
    if (!checkStation(from, error) || !checkStation(to, error)) {
        return false;
    }
    if (!network_.map().hasEdge(from, to)) {
        return fail(error, "edge does not exist");
    }
    for (std::size_t i = 0; i < network_.lines().size(); ++i) {
        const Line& line = network_.lines()[i];
        for (int j = 0; j + 1 < line.stationCount(); ++j) {
            const Index a = line.stations()[static_cast<std::size_t>(j)];
            const Index b = line.stations()[static_cast<std::size_t>(j + 1)];
            if ((a == from && b == to) || (a == to && b == from)) {
                return fail(error, "edge is used by line " + line.name() +
                                       "; remove the line first");
            }
        }
    }
    pushUndo();
    if (!network_.map().removeEdge(from, to)) {
        rollbackUndo();
        return fail(error, "failed to remove edge");
    }
    dirty_ = true;
    return true;
}

int Editor::addLine(const std::string& name, std::uint32_t color, std::string* error) {
    pushUndo();
    Line line(-1, name.empty() ? "Line " + std::to_string(network_.lines().size() + 1) : name,
              color);
    line.ops().dwellSec = 30.0;
    line.ops().headway.push_back(HeadwayPeriod{0, 86400, 300});
    const int index = network_.addLine(static_cast<Line&&>(line));
    dirty_ = true;
    (void)error;
    return index;
}

bool Editor::removeLine(int line, std::string* error) {
    if (!checkLine(line, error)) {
        return false;
    }
    pushUndo();
    if (!network_.removeLine(line)) {
        rollbackUndo();
        return fail(error, "failed to remove line");
    }
    dirty_ = true;
    return true;
}

bool Editor::appendStop(int line, Index station, std::string* error) {
    if (!checkLine(line, error) || !checkStation(station, error)) {
        return false;
    }
    const Line& current = network_.lines()[static_cast<std::size_t>(line)];
    if (current.containsStation(station)) {
        return fail(error, "line already contains this station");
    }
    if (current.stationCount() > 0) {
        const Index last = current.stations()[current.stations().size() - 1];
        if (!network_.map().hasEdge(last, station)) {
            return fail(error, "consecutive stations have no map edge");
        }
    }

    pushUndo();
    Line& target = network_.lines()[static_cast<std::size_t>(line)];
    target.stations().push_back(station);
    if (target.stationCount() >= 2) {
        target.ops().segmentRunSec.push_back(kDefaultSegmentRunSec);
    }
    dirty_ = true;
    return true;
}

bool Editor::removeLastStop(int line, std::string* error) {
    if (!checkLine(line, error)) {
        return false;
    }
    pushUndo();
    Line& target = network_.lines()[static_cast<std::size_t>(line)];
    if (!target.stations().empty()) {
        target.stations().pop_back();
    }
    if (!target.ops().segmentRunSec.empty()) {
        target.ops().segmentRunSec.pop_back();
    }
    dirty_ = true;
    return true;
}

bool Editor::setLineStations(int line, const DynamicArray<Index>& stations,
                             std::string* error) {
    if (!checkLine(line, error)) {
        return false;
    }
    for (std::size_t i = 0; i < stations.size(); ++i) {
        if (!checkStation(stations[i], error)) {
            return false;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (stations[j] == stations[i]) {
                return fail(error, "line repeats a station");
            }
        }
        if (i > 0 && !network_.map().hasEdge(stations[i - 1], stations[i])) {
            return fail(error, "consecutive stations have no map edge");
        }
    }

    Line candidate = network_.lines()[static_cast<std::size_t>(line)];
    candidate.setStations(stations);
    candidate.ops().segmentRunSec.clear();
    for (std::size_t i = 1; i < stations.size(); ++i) {
        candidate.ops().segmentRunSec.push_back(kDefaultSegmentRunSec);
    }
    if (stations.size() >= 2) {
        std::string validateError;
        if (!candidate.validate(network_.map(), &validateError)) {
            return fail(error, validateError);
        }
    }

    pushUndo();
    network_.lines()[static_cast<std::size_t>(line)] = static_cast<Line&&>(candidate);
    dirty_ = true;
    return true;
}

bool Editor::replaceLine(int line, Line updated, std::string* error) {
    if (!checkLine(line, error)) {
        return false;
    }
    updated.setId(line);
    std::string validateError;
    if (!updated.validate(network_.map(), &validateError)) {
        return fail(error, validateError);
    }
    pushUndo();
    network_.lines()[static_cast<std::size_t>(line)] =
        static_cast<Line&&>(updated);
    dirty_ = true;
    return true;
}

bool Editor::setTransferTime(Index station, int fromLine, int toLine, double seconds,
                             std::string* error) {
    if (!checkStation(station, error) || !checkLine(fromLine, error) ||
        !checkLine(toLine, error)) {
        return false;
    }
    if (fromLine == toLine) {
        return fail(error, "transfer lines must differ");
    }
    if (seconds < 0.0) {
        return fail(error, "transfer time cannot be negative");
    }
    pushUndo();
    if (!network_.setTransferTime(station, fromLine, toLine, seconds)) {
        rollbackUndo();
        return fail(error, "failed to set transfer time");
    }
    dirty_ = true;
    return true;
}

bool Editor::setTransferTimes(const DynamicArray<TransferRecord>& records,
                              std::string* error) {
    for (std::size_t i = 0; i < records.size(); ++i) {
        const TransferRecord& record = records[i];
        if (!checkStation(record.station, error) || !checkLine(record.fromLine, error) ||
            !checkLine(record.toLine, error)) {
            return false;
        }
        if (record.fromLine == record.toLine) {
            return fail(error, "transfer lines must differ");
        }
        if (record.seconds < 0.0) {
            return fail(error, "transfer time cannot be negative");
        }
    }
    if (records.empty()) {
        return true;
    }
    pushUndo();
    for (std::size_t i = 0; i < records.size(); ++i) {
        const TransferRecord& record = records[i];
        network_.setTransferTime(record.station, record.fromLine, record.toLine,
                                 record.seconds);
    }
    dirty_ = true;
    return true;
}

void Editor::pushUndo() {
    undo_.push_back(network_);
    redo_.clear();
}

void Editor::rollbackUndo() {
    if (!undo_.empty()) {
        undo_.pop_back();
    }
}

bool Editor::undo() {
    if (undo_.empty()) {
        return false;
    }
    redo_.push_back(static_cast<Network&&>(network_));
    network_ = static_cast<Network&&>(undo_.back());
    undo_.pop_back();
    dirty_ = true;
    return true;
}

bool Editor::redo() {
    if (redo_.empty()) {
        return false;
    }
    undo_.push_back(static_cast<Network&&>(network_));
    network_ = static_cast<Network&&>(redo_.back());
    redo_.pop_back();
    dirty_ = true;
    return true;
}

}  // namespace uc
