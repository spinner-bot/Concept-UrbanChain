#include "nav/router.h"

#include <cmath>

#include "ds/binary_heap.h"

namespace uc {
namespace {

int stationPosition(const Line& line, Index station) {
    for (std::size_t i = 0; i < line.stations().size(); ++i) {
        if (line.stations()[i] == station) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace

Journey Router::findRoute(const Network& network, Index origin, Index destination,
                          double departTimeSec) const {
    Journey journey;
    journey.departTime = departTimeSec;

    const Map& map = network.map();
    if (origin < 0 || destination < 0 || origin >= map.stationCount() ||
        destination >= map.stationCount()) {
        return journey;
    }
    if (origin == destination) {
        journey.found = true;
        journey.arriveTime = departTimeSec;
        return journey;
    }

    const int lineCount = static_cast<int>(network.lines().size());
    if (lineCount == 0) {
        return journey;
    }

    const std::size_t stateCount =
        static_cast<std::size_t>(map.stationCount()) * static_cast<std::size_t>(lineCount);
    DynamicArray<double> dist;
    dist.resize(stateCount);
    DynamicArray<int> previous;
    previous.resize(stateCount);
    for (std::size_t i = 0; i < stateCount; ++i) {
        dist[i] = kInfinity;
        previous[i] = -1;
    }

    BinaryMinHeap heap;
    auto relax = [&](Index station, int line, double time, int fromState) {
        const std::size_t state = static_cast<std::size_t>(station) * lineCount + line;
        if (time + 1e-9 < dist[state]) {
            dist[state] = time;
            previous[state] = fromState;
            heap.push(time, static_cast<int>(state));
        }
    };

    const int departSec = static_cast<int>(std::floor(departTimeSec));
    for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
        const Line& line = network.lines()[static_cast<std::size_t>(lineIndex)];
        if (!line.containsStation(origin)) {
            continue;
        }
        const int headway = line.ops().headwayAt(departSec);
        if (headway <= 0) {
            continue;
        }
        relax(origin, lineIndex, departTimeSec + headway * 0.5, -1);
    }

    while (!heap.empty()) {
        const HeapItem item = heap.pop();
        const int state = item.value;
        if (item.key > dist[static_cast<std::size_t>(state)] + 1e-9) {
            continue;
        }

        const Index station = state / lineCount;
        const int lineIndex = state % lineCount;
        const Line& line = network.lines()[static_cast<std::size_t>(lineIndex)];
        const int position = stationPosition(line, station);
        if (position < 0) {
            continue;
        }

        const double dwell = line.ops().dwellSec;
        if (position + 1 < line.stationCount()) {
            const double ride = dwell + line.ops().segmentRunSec[static_cast<std::size_t>(position)];
            relax(line.stations()[static_cast<std::size_t>(position + 1)], lineIndex,
                  item.key + ride, state);
        }
        if (position - 1 >= 0) {
            const double ride =
                dwell + line.ops().segmentRunSec[static_cast<std::size_t>(position - 1)];
            relax(line.stations()[static_cast<std::size_t>(position - 1)], lineIndex,
                  item.key + ride, state);
        }

        for (int other = 0; other < lineCount; ++other) {
            if (other == lineIndex) {
                continue;
            }
            const Line& target = network.lines()[static_cast<std::size_t>(other)];
            if (!target.containsStation(station)) {
                continue;
            }
            const double walk = network.transferTime(station, lineIndex, other);
            const int arrivalAtPlatform =
                static_cast<int>(std::floor(item.key + walk));
            const int headway = target.ops().headwayAt(arrivalAtPlatform);
            if (headway <= 0) {
                continue;
            }
            const double boardTime = item.key + walk + headway * 0.5;
            relax(station, other, boardTime, state);
        }
    }

    int bestState = -1;
    double bestTime = kInfinity;
    for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
        const std::size_t state =
            static_cast<std::size_t>(destination) * lineCount + lineIndex;
        if (dist[state] < bestTime) {
            bestTime = dist[state];
            bestState = static_cast<int>(state);
        }
    }
    if (bestState < 0) {
        return journey;
    }

    DynamicArray<int> chain;
    int current = bestState;
    while (current >= 0) {
        chain.push_back(current);
        if (chain.size() > stateCount) {
            return journey;
        }
        current = previous[static_cast<std::size_t>(current)];
    }
    for (std::size_t i = 0, j = chain.size(); i + 1 < j; ++i, --j) {
        const int tmp = chain[i];
        chain[i] = chain[j - 1];
        chain[j - 1] = tmp;
    }

    std::size_t index = 0;
    while (index < chain.size()) {
        const int lineIndex = chain[index] % lineCount;
        const int firstState = chain[index];
        int lastState = chain[index];
        std::size_t next = index + 1;
        while (next < chain.size() && chain[next] % lineCount == lineIndex) {
            lastState = chain[next];
            ++next;
        }
        JourneyLeg leg;
        leg.line = lineIndex;
        leg.fromStation = firstState / lineCount;
        leg.toStation = lastState / lineCount;
        leg.boardTime = dist[static_cast<std::size_t>(firstState)];
        leg.alightTime = dist[static_cast<std::size_t>(lastState)];
        journey.legs.push_back(leg);
        index = next;
    }

    journey.found = true;
    journey.arriveTime = bestTime;
    journey.transfers =
        journey.legs.empty() ? 0 : static_cast<int>(journey.legs.size()) - 1;
    return journey;
}

}  // namespace uc
