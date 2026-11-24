#include "plan/evaluator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

#include "ds/hash_map.h"
#include "nav/router.h"

namespace uc {
namespace plan {
namespace {

double lineGeometryLength(const Map& map, const Line& line) {
    double total = 0.0;
    for (int i = 0; i + 1 < line.stationCount(); ++i) {
        total += euclideanDistance(
            map.station(line.stations()[static_cast<std::size_t>(i)]).position,
            map.station(line.stations()[static_cast<std::size_t>(i + 1)]).position);
    }
    return total;
}

std::uint64_t edgeKey(Index a, Index b) {
    const std::uint32_t low =
        static_cast<std::uint32_t>(std::min(a, b));
    const std::uint32_t high =
        static_cast<std::uint32_t>(std::max(a, b));
    return (static_cast<std::uint64_t>(low) << 32) | high;
}

Network buildEvaluationNetwork(const Map& map, const DynamicArray<Line>& lines,
                               const EvalParams& params) {
    Network network;
    network.map() = map;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        Line line = lines[i];
        if (params.overrideLineOps) {
            const int stationCount = line.stationCount();
            line.ops().segmentRunSec.clear();
            for (int j = 0; j + 1 < stationCount; ++j) {
                line.ops().segmentRunSec.push_back(params.ops.segmentRunSec);
            }
            line.ops().dwellSec = params.ops.dwellSec;
            line.ops().headway.clear();
            line.ops().headway.push_back(HeadwayPeriod{0, 86400, params.ops.headwaySec});
        }
        network.addLine(static_cast<Line&&>(line));
    }

    if (params.overrideLineOps) {
        for (Index station = 0; station < map.stationCount(); ++station) {
            DynamicArray<int> serving;
            for (std::size_t line = 0; line < network.lines().size(); ++line) {
                if (network.lines()[line].containsStation(station)) {
                    serving.push_back(static_cast<int>(line));
                }
            }
            for (std::size_t a = 0; a < serving.size(); ++a) {
                for (std::size_t b = 0; b < serving.size(); ++b) {
                    if (a == b) {
                        continue;
                    }
                    network.setTransferTime(station, serving[a], serving[b],
                                            params.ops.transferSec);
                }
            }
        }
    }
    return network;
}

}  // namespace

Metrics evaluateNetwork(const Network& network, const WeightResult& weights,
                        const DemandResult& demand, const EvalParams& params) {
    const auto startTime = std::chrono::steady_clock::now();
    Metrics metrics;
    const Map& map = network.map();
    const int stationCount = map.stationCount();
    const int lineCount = static_cast<int>(network.lines().size());
    metrics.stationCount = stationCount;
    metrics.lineCount = lineCount;

    if (static_cast<int>(weights.weights.size()) != stationCount ||
        demand.stationCount != stationCount ||
        static_cast<int>(demand.od.size()) != stationCount * stationCount) {
        metrics.valid = false;
        return metrics;
    }

    // R1：加权覆盖率。
    double totalWeight = 0.0;
    double coveredWeight = 0.0;
    DynamicArray<unsigned char> covered;
    covered.resize(static_cast<std::size_t>(stationCount));
    for (int i = 0; i < stationCount; ++i) {
        covered[static_cast<std::size_t>(i)] = 0;
        totalWeight += weights.weights[static_cast<std::size_t>(i)];
    }
    for (std::size_t i = 0; i < network.lines().size(); ++i) {
        const Line& line = network.lines()[i];
        for (std::size_t j = 0; j < line.stations().size(); ++j) {
            const Index station = line.stations()[j];
            if (station >= 0 && station < stationCount) {
                covered[static_cast<std::size_t>(station)] = 1;
            }
        }
    }
    for (int i = 0; i < stationCount; ++i) {
        if (covered[static_cast<std::size_t>(i)]) {
            coveredWeight += weights.weights[static_cast<std::size_t>(i)];
        }
    }
    metrics.r1 = totalWeight > 1e-12 ? coveredWeight / totalWeight : 0.0;

    // R2：边并集成本 + 站台成本。
    HashMap<std::uint64_t, double, IntHash> usedEdges;
    double edgeCost = 0.0;
    DynamicArray<int> platformCount;
    platformCount.resize(static_cast<std::size_t>(stationCount));
    for (int i = 0; i < stationCount; ++i) {
        platformCount[static_cast<std::size_t>(i)] = 0;
    }
    for (std::size_t i = 0; i < network.lines().size(); ++i) {
        const Line& line = network.lines()[i];
        for (int j = 0; j < line.stationCount(); ++j) {
            const Index station = line.stations()[static_cast<std::size_t>(j)];
            if (station >= 0 && station < stationCount) {
                ++platformCount[static_cast<std::size_t>(station)];
            }
        }
        for (int j = 0; j + 1 < line.stationCount(); ++j) {
            const Index a = line.stations()[static_cast<std::size_t>(j)];
            const Index b = line.stations()[static_cast<std::size_t>(j + 1)];
            if (!map.hasEdge(a, b)) {
                metrics.valid = false;
                continue;
            }
            const std::uint64_t key = edgeKey(a, b);
            if (!usedEdges.contains(key)) {
                usedEdges.insert(key, map.edgeCost(a, b));
                edgeCost += map.edgeCost(a, b);
            }
        }
    }
    double platformWeight = 0.0;
    for (int i = 0; i < stationCount; ++i) {
        platformWeight += platformCount[static_cast<std::size_t>(i)] *
                          weights.weights[static_cast<std::size_t>(i)];
    }
    metrics.r2 = edgeCost + params.kStation * platformWeight;

    // R5 / R6：基于相邻站直线距离的线路长度。
    double detourSum = 0.0;
    int detourCount = 0;
    double shortPenalty = 0.0;
    for (std::size_t i = 0; i < network.lines().size(); ++i) {
        const Line& line = network.lines()[i];
        if (line.stationCount() < 2) {
            continue;
        }
        const double length = lineGeometryLength(map, line);
        const double direct = euclideanDistance(
            map.station(line.stations()[0]).position,
            map.station(line.stations()[line.stations().size() - 1]).position);
        if (direct > 1e-9) {
            detourSum += length / direct;
            ++detourCount;
        }
        if (params.shortBeta > 1e-12) {
            const double ratio = std::max(0.0, 1.0 - length / params.shortBeta);
            shortPenalty += params.shortAlpha * ratio * ratio;
        }
    }
    metrics.r5 = detourCount > 0 ? detourSum / detourCount : 0.0;
    metrics.r6 = shortPenalty;

    // R3 / R4：需求加权的最短期望出行时间与换乘次数。
    double sumTime = 0.0;
    double sumTransfers = 0.0;
    double denominator = 0.0;
    bool hasUnreachable = false;
    if (stationCount >= 2) {
        Router router;
        const double departTime = params.ops.initialTimeSec;
        for (int i = 0; i < stationCount; ++i) {
            for (int j = i + 1; j < stationCount; ++j) {
                const double od =
                    demand.od[static_cast<std::size_t>(i) * stationCount +
                              static_cast<std::size_t>(j)];
                if (od <= 0.0) {
                    continue;
                }
                ++metrics.odPairs;
                if (params.unreachable != UnreachablePolicy::Exclude) {
                    denominator += od;
                }
                const Journey journey = router.findRoute(network, i, j, departTime);
                if (journey.found) {
                    ++metrics.reachablePairs;
                    const double travel = std::max(0.0, journey.arriveTime - departTime);
                    sumTime += od * travel;
                    sumTransfers += od * journey.transfers;
                    if (params.unreachable == UnreachablePolicy::Exclude) {
                        denominator += od;
                    }
                } else {
                    ++metrics.unreachablePairs;
                    if (params.unreachable == UnreachablePolicy::Penalty) {
                        sumTime += od * params.unreachablePenaltySec;
                    } else if (params.unreachable == UnreachablePolicy::Infinite) {
                        hasUnreachable = true;
                    }
                }
            }
        }
    }

    metrics.odTotal = denominator;
    if (hasUnreachable) {
        metrics.r3 = kInfinity;
        metrics.r4 = kInfinity;
    } else if (denominator > 1e-12) {
        metrics.r3 = sumTime / denominator;
        metrics.r4 = sumTransfers / denominator;
    } else {
        metrics.r3 = 0.0;
        metrics.r4 = 0.0;
    }

    const auto endTime = std::chrono::steady_clock::now();
    metrics.elapsedMs =
        std::chrono::duration<double, std::milli>(endTime - startTime).count();
    return metrics;
}

Metrics evaluate(const Map& map, const DynamicArray<Line>& lines,
                 const WeightResult& weights, const DemandResult& demand,
                 const EvalParams& params) {
    const Network network = buildEvaluationNetwork(map, lines, params);
    return evaluateNetwork(network, weights, demand, params);
}

}  // namespace plan
}  // namespace uc
