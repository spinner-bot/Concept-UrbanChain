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
    return addEdge(from, to, distance, distance, error);
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
