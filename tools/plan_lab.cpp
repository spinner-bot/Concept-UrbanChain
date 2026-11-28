#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/map.h"
#include "core/network.h"
#include "ds/sort.h"
#include "ds/union_find.h"
#include "io/line_io.h"
#include "io/map_io.h"
#include "plan/audit.h"
#include "plan/corridor.h"
#include "plan/demand.h"
#include "plan/evaluator.h"
#include "plan/search.h"
#include "plan/weights.h"

using namespace uc;
using namespace uc::plan;

namespace {

Map makeGridMap(int cols, int rows) {
    Map map;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            Station station;
            station.id = "S" + std::to_string(r * cols + c);
            station.name = station.id;
            station.position = Vec2{static_cast<double>(c), static_cast<double>(r)};
            map.addStation(station);
        }
    }
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const Index index = r * cols + c;
            if (c + 1 < cols) {
                map.addEdge(index, index + 1, 1.0, 1.0);
            }
            if (r + 1 < rows) {
                map.addEdge(index, index + cols, 1.0, 1.0);
            }
        }
    }
    return map;
}

Map makeClusteredMap(int stationsPerCluster, int clusterCount, int seed) {
    Map map;
    std::mt19937 rng(static_cast<std::uint32_t>(seed * 7919 + 17));
    std::normal_distribution<double> noise(0.0, 1.15);
    for (int cluster = 0; cluster < clusterCount; ++cluster) {
        const double angle =
            6.283185307179586 * static_cast<double>(cluster) / std::max(1, clusterCount);
        const double radius = clusterCount <= 1 ? 0.0 : 11.0;
        const double centerX = std::cos(angle) * radius;
        const double centerY = std::sin(angle) * radius;
        for (int i = 0; i < stationsPerCluster; ++i) {
            Station station;
            station.id = "S" + std::to_string(cluster) + "_" + std::to_string(i);
            station.name = station.id;
            station.position =
                Vec2{centerX + noise(rng), centerY + noise(rng)};
            map.addStation(station);
        }
    }
    return map;
}

Map makeSimulatedCity(int centerCount, int stationsPerCenter, double sigma, int seed,
                      DynamicArray<Line>* baselineLines = nullptr) {
    Map map;
    std::mt19937 rng(static_cast<std::uint32_t>(seed * 7919 + 101));
    std::normal_distribution<double> radial(0.0, sigma);
    std::uniform_real_distribution<double> jitter(-0.6, 0.6);
    std::uniform_real_distribution<double> costNoise(-0.18, 0.18);

    struct Center {
        Vec2 position;
        int hub = kInvalidIndex;
        std::vector<int> corridor;
    };
    std::vector<Center> centers;

    auto addStation = [&map](double x, double y, const std::string& prefix) {
        Station station;
        station.id = prefix + std::to_string(map.stationCount());
        station.name = station.id;
        station.position = Vec2{x, y};
        Index index = kInvalidIndex;
        map.addStation(station, &index);
        return index;
    };
    auto addEdgeNoisy = [&map, &rng, &costNoise](Index a, Index b) {
        if (a == b || map.hasEdge(a, b)) {
            return;
        }
        const double length =
            euclideanDistance(map.station(a).position, map.station(b).position);
        if (length < 1e-9) {
            return;
        }
        // 单位距离成本围绕 1.0 上下浮动，模拟不同建设难度。
        const double unitCost = std::max(0.01, 1.0 + costNoise(rng));
        map.addEdge(a, b, unitCost, length);
    };

    for (int c = 0; c < centerCount; ++c) {
        const double angle =
            6.283185307179586 * static_cast<double>(c) / std::max(1, centerCount);
        const double radius = centerCount <= 1 ? 0.0 : 13.0;
        Center center;
        center.position =
            Vec2{std::cos(angle) * radius, std::sin(angle) * radius};
        center.hub = addStation(center.position.x, center.position.y, "H");
        for (int i = 0; i < stationsPerCenter; ++i) {
            const double x = center.position.x + radial(rng);
            const double y = center.position.y + radial(rng);
            addStation(x, y, "S");
        }
        centers.push_back(center);
    }

    if (centerCount == 1) {
        // 单中心城市：从中心 hub 向若干方向延伸放射状主干走廊。
        const int spokeCount = 5;
        for (int s = 0; s < spokeCount; ++s) {
            const double angle =
                6.283185307179586 * static_cast<double>(s) / spokeCount;
            int previous = centers[0].hub;
            DynamicArray<Index> spoke;
            spoke.push_back(previous);
            for (int k = 1; k <= 7; ++k) {
                const double radius = 2.5 + static_cast<double>(k) * 2.4;
                const double x = std::cos(angle) * radius + jitter(rng);
                const double y = std::sin(angle) * radius + jitter(rng);
                const int station = addStation(x, y, "C");
                addEdgeNoisy(previous, station);
                previous = station;
                spoke.push_back(station);
            }
            if (baselineLines != nullptr) {
                Line line(0, "spoke", 0x2563ebu);
                line.setStations(static_cast<DynamicArray<Index>&&>(spoke));
                line.ops().dwellSec = 30.0;
                line.ops().headway.push_back(HeadwayPeriod{0, 86400, 300});
                for (int i = 0; i + 1 < line.stationCount(); ++i) {
                    line.ops().segmentRunSec.push_back(120.0);
                }
                baselineLines->push_back(static_cast<Line&&>(line));
            }
        }
    } else {
        // 多中心城市：中心 hub 之间用环形主干走廊串联。
        for (int c = 0; c < centerCount; ++c) {
            Center& from = centers[static_cast<std::size_t>(c)];
            Center& to = centers[static_cast<std::size_t>((c + 1) % centerCount)];
            from.corridor.push_back(from.hub);
            for (int k = 1; k <= 6; ++k) {
                const double t = static_cast<double>(k) / 7.0;
                const double x = from.position.x + (to.position.x - from.position.x) * t +
                                 jitter(rng);
                const double y = from.position.y + (to.position.y - from.position.y) * t +
                                 jitter(rng);
                from.corridor.push_back(addStation(x, y, "C"));
            }
            from.corridor.push_back(to.hub);
            for (std::size_t i = 0; i + 1 < from.corridor.size(); ++i) {
                addEdgeNoisy(from.corridor[i], from.corridor[i + 1]);
            }
            if (baselineLines != nullptr) {
                Line line(0, "arc", 0x7c3aedu);
                for (std::size_t i = 0; i < from.corridor.size(); ++i) {
                    line.stations().push_back(from.corridor[i]);
                }
                line.ops().dwellSec = 30.0;
                line.ops().headway.push_back(HeadwayPeriod{0, 86400, 300});
                for (int i = 0; i + 1 < line.stationCount(); ++i) {
                    line.ops().segmentRunSec.push_back(120.0);
                }
                baselineLines->push_back(static_cast<Line&&>(line));
            }
        }
    }

    // k 近邻连接，形成不规则路网。
    const int count = map.stationCount();
    const int kNeighbors = 4;
    for (int i = 0; i < count; ++i) {
        std::vector<std::pair<double, int>> distances;
        for (int j = 0; j < count; ++j) {
            if (i == j) {
                continue;
            }
            distances.push_back(
                std::make_pair(euclideanDistance(map.station(i).position,
                                                 map.station(j).position),
                               j));
        }
        std::sort(distances.begin(), distances.end());
        for (int k = 0; k < std::min(kNeighbors, static_cast<int>(distances.size()));
             ++k) {
            addEdgeNoisy(i, distances[static_cast<std::size_t>(k)].second);
        }
    }

    // 连通修复：合并距离最近的不同分量。
    UnionFind sets(count);
    for (std::size_t i = 0; i < map.edges().size(); ++i) {
        sets.unite(map.edges()[i].from, map.edges()[i].to);
    }
    while (sets.count() > 1) {
        int bestA = -1;
        int bestB = -1;
        double bestDistance = kInfinity;
        for (int i = 0; i < count; ++i) {
            for (int j = i + 1; j < count; ++j) {
                if (sets.find(i) == sets.find(j)) {
                    continue;
                }
                const double d = euclideanDistance(map.station(i).position,
                                                   map.station(j).position);
                if (d < bestDistance) {
                    bestDistance = d;
                    bestA = i;
                    bestB = j;
                }
            }
        }
        if (bestA < 0) {
            break;
        }
        addEdgeNoisy(bestA, bestB);
        sets.unite(bestA, bestB);
    }
    return map;
}

DynamicArray<Line> makeGridLines(const Map& map, int cols, int rows, bool horizontal,
                                 bool vertical) {
    (void)map;
    DynamicArray<Line> lines;
    const std::uint32_t palette[] = {0xef4444u, 0x2563ebu, 0x16a34au, 0xf59e0bu,
                                     0x7c3aedu, 0x0891b2u, 0xdb2777u, 0x65a30du};
    int colorIndex = 0;
    auto addLine = [&](const std::string& name, int start, int step, int count) {
        Line line(0, name, palette[colorIndex % 8]);
        ++colorIndex;
        for (int i = 0; i < count; ++i) {
            line.stations().push_back(start + i * step);
        }
        line.ops().dwellSec = 30.0;
        line.ops().headway.push_back(HeadwayPeriod{0, 86400, 300});
        for (int i = 0; i + 1 < count; ++i) {
            line.ops().segmentRunSec.push_back(120.0);
        }
        lines.push_back(static_cast<Line&&>(line));
    };

    if (horizontal) {
        for (int r = 0; r < rows; ++r) {
            addLine("H" + std::to_string(r), r * cols, 1, cols);
        }
    }
    if (vertical) {
        for (int c = 0; c < cols; ++c) {
            addLine("V" + std::to_string(c), c, cols, rows);
        }
    }
    return lines;
}

WeightParams makeWeightParams(int k, int seed) {
    WeightParams params;
    params.clusters = k;
    params.seed = seed;
    return params;
}

DemandResult makeDemand(const Map& map, const WeightResult& weights, double beta) {
    DemandParams params;
    params.beta = beta;
    return computeDemand(map, weights, params);
}

EvalParams makeEvalParams(UnreachablePolicy policy, int headwaySec, double dwellSec,
                          double transferSec) {
    EvalParams params;
    params.unreachable = policy;
    params.ops.headwaySec = headwaySec;
    params.ops.dwellSec = dwellSec;
    params.ops.transferSec = transferSec;
    params.ops.initialTimeSec = 8.0 * 3600.0;
    params.kStation = 1.0;
    params.shortAlpha = 1.0;
    params.shortBeta = 2.0;
    params.overrideLineOps = true;
    return params;
}

std::string number(double value) {
    if (std::isinf(value)) {
        return "inf";
    }
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(4) << value;
    return stream.str();
}

void printMetrics(const Metrics& metrics) {
    std::cout << "  R1=" << number(metrics.r1) << " R2=" << number(metrics.r2)
              << " R3=" << number(metrics.r3) << " R4=" << number(metrics.r4)
              << " R5=" << number(metrics.r5) << " R6=" << number(metrics.r6) << '\n';
    std::cout << "  valid=" << (metrics.valid ? "yes" : "no")
              << " feasible=" << (metrics.feasible ? "yes" : "no")
              << " connected=" << (metrics.connected ? "yes" : "no")
              << " components=" << metrics.networkComponents
              << " covered=" << metrics.coveredStations << "/" << metrics.stationCount
              << " lines=" << metrics.lineCount
              << " odPairs=" << metrics.odPairs
              << " reachable=" << metrics.reachablePairs
              << " unreachable=" << metrics.unreachablePairs
              << " odTotal=" << number(metrics.odTotal)
              << " evalMs=" << number(metrics.elapsedMs) << '\n';
}

void printWeights(const Map& map, const WeightResult& result, int top) {
    double sum = 0.0;
    double minimum = kInfinity;
    double maximum = -kInfinity;
    for (std::size_t i = 0; i < result.weights.size(); ++i) {
        sum += result.weights[i];
        minimum = std::min(minimum, result.weights[i]);
        maximum = std::max(maximum, result.weights[i]);
    }
    std::cout << "stations=" << map.stationCount()
              << " clusters=" << result.clusterCount
              << " iterations=" << result.iterations
              << " restarts=" << result.restartsTried
              << " inertia=" << number(result.inertia)
              << " R=" << number(result.scaleR)
              << " global=(" << number(result.globalCenter.x) << ","
              << number(result.globalCenter.y) << ")\n";
    std::cout << "weight mean=" << number(sum / std::max(1, map.stationCount()))
              << " min=" << number(minimum) << " max=" << number(maximum) << '\n';

    DynamicArray<int> counts;
    counts.resize(static_cast<std::size_t>(result.clusterCount));
    for (int i = 0; i < result.clusterCount; ++i) {
        counts[static_cast<std::size_t>(i)] = 0;
    }
    for (std::size_t i = 0; i < result.clusterOf.size(); ++i) {
        ++counts[static_cast<std::size_t>(result.clusterOf[i])];
    }
    std::cout << "cluster sizes:";
    for (int i = 0; i < result.clusterCount; ++i) {
        std::cout << ' ' << counts[static_cast<std::size_t>(i)];
    }
    std::cout << '\n';

    DynamicArray<int> order;
    for (int i = 0; i < map.stationCount(); ++i) {
        order.push_back(i);
    }
    quickSort(order, [&result](int a, int b) {
        return result.weights[static_cast<std::size_t>(a)] >
               result.weights[static_cast<std::size_t>(b)];
    });
    std::cout << "top stations:\n";
    for (int i = 0; i < std::min(top, static_cast<int>(order.size())); ++i) {
        const int index = order[static_cast<std::size_t>(i)];
        std::cout << "  " << map.station(index).id << "  w="
                  << number(result.weights[static_cast<std::size_t>(index)])
                  << "  pos=(" << map.station(index).position.x << ","
                  << map.station(index).position.y << ")\n";
    }
}

void modeWeights(int cols, int rows, int k, int seed) {
    const Map map = makeGridMap(cols, rows);
    const WeightResult weights = computeWeights(map, makeWeightParams(k, seed));
    printWeights(map, weights, 10);
}

void modeWeightStability(int cols, int rows, int k) {
    const Map map = makeGridMap(cols, rows);
    const WeightResult base = computeWeights(map, makeWeightParams(k, 1));
    std::cout << "seed | maxRelDiff | meanAbsDiff | clusters\n";
    for (int seed = 1; seed <= 10; ++seed) {
        const WeightResult other = computeWeights(map, makeWeightParams(k, seed));
        double maxRel = 0.0;
        double meanAbs = 0.0;
        for (std::size_t i = 0; i < base.weights.size(); ++i) {
            const double a = base.weights[i];
            const double b = other.weights[i];
            const double diff = std::fabs(a - b);
            maxRel = std::max(maxRel, diff / std::max(1e-12, std::fabs(a)));
            meanAbs += diff;
        }
        meanAbs /= std::max(1, static_cast<int>(base.weights.size()));
        std::cout << std::setw(4) << seed << " | " << number(maxRel) << " | "
                  << number(meanAbs) << " | " << other.clusterCount << '\n';
    }
}

void modeClusteredStability(int stationsPerCluster, int clusterCount, int k) {
    const Map map = makeClusteredMap(stationsPerCluster, clusterCount, 1);
    const WeightResult base = computeWeights(map, makeWeightParams(k, 1));
    std::cout << "clustered map: " << map.stationCount() << " stations, "
              << clusterCount << " spatial clusters, stations/cluster="
              << stationsPerCluster << '\n';
    printWeights(map, base, 10);
    std::cout << "seed | maxRelDiff | meanAbsDiff | clusters | cluster sizes\n";
    for (int seed = 1; seed <= 10; ++seed) {
        const WeightResult other = computeWeights(map, makeWeightParams(k, seed));
        double maxRel = 0.0;
        double meanAbs = 0.0;
        for (std::size_t i = 0; i < base.weights.size(); ++i) {
            const double a = base.weights[i];
            const double b = other.weights[i];
            const double diff = std::fabs(a - b);
            maxRel = std::max(maxRel, diff / std::max(1e-12, std::fabs(a)));
            meanAbs += diff;
        }
        meanAbs /= std::max(1, static_cast<int>(base.weights.size()));
        DynamicArray<int> sizes;
        sizes.resize(static_cast<std::size_t>(other.clusterCount));
        for (int c = 0; c < other.clusterCount; ++c) {
            sizes[static_cast<std::size_t>(c)] = 0;
        }
        for (std::size_t i = 0; i < other.clusterOf.size(); ++i) {
            ++sizes[static_cast<std::size_t>(other.clusterOf[i])];
        }
        std::cout << std::setw(4) << seed << " | " << number(maxRel) << " | "
                  << number(meanAbs) << " | " << other.clusterCount << " |";
        for (int c = 0; c < other.clusterCount; ++c) {
            std::cout << ' ' << sizes[static_cast<std::size_t>(c)];
        }
        std::cout << '\n';
    }
}

void modeTiming(int k) {
    std::cout << "cols | stations | lines | odPairs | reachable | unreachable | evalMs | "
                 "R1 | R2 | R3 | R4 | R5 | R6\n";
    const int sizes[] = {6, 8, 10, 12};
    for (int size : sizes) {
        const Map map = makeGridMap(size, size);
        const DynamicArray<Line> lines = makeGridLines(map, size, size, true, true);
        const WeightResult weights = computeWeights(map, makeWeightParams(k, 1));
        const DemandResult demand = makeDemand(map, weights, 1.0);
        const EvalParams params = makeEvalParams(UnreachablePolicy::Exclude, 300, 30.0, 120.0);
        const Metrics metrics = evaluate(map, lines, weights, demand, params);
        std::cout << size << " | " << metrics.stationCount << " | " << metrics.lineCount
                  << " | " << metrics.odPairs << " | " << metrics.reachablePairs << " | "
                  << metrics.unreachablePairs << " | " << number(metrics.elapsedMs) << " | "
                  << number(metrics.r1) << " | " << number(metrics.r2) << " | "
                  << number(metrics.r3) << " | " << number(metrics.r4) << " | "
                  << number(metrics.r5) << " | " << number(metrics.r6) << '\n';
    }
}

void modePolicy(int cols, int rows, int k) {
    const Map map = makeGridMap(cols, rows);
    const DynamicArray<Line> lines = makeGridLines(map, cols, rows, true, false);
    const WeightResult weights = computeWeights(map, makeWeightParams(k, 1));
    const DemandResult demand = makeDemand(map, weights, 1.0);
    const ConnectivityRequirement requirements[] = {
        ConnectivityRequirement::AllStations,
        ConnectivityRequirement::CoveredStations,
        ConnectivityRequirement::None};
    const char* names[] = {"AllStations", "CoveredStations", "None"};
    std::cout << "horizontal lines only: " << lines.size() << " lines, "
              << map.stationCount() << " stations\n";
    for (int i = 0; i < 3; ++i) {
        EvalParams params = makeEvalParams(UnreachablePolicy::Exclude, 300, 30.0, 120.0);
        params.connectivity = requirements[i];
        const Metrics metrics = evaluate(map, lines, weights, demand, params);
        std::cout << names[i] << ":\n";
        printMetrics(metrics);
    }
}

void modeOps(int cols, int rows, int k) {
    const Map map = makeGridMap(cols, rows);
    const DynamicArray<Line> lines = makeGridLines(map, cols, rows, true, true);
    const WeightResult weights = computeWeights(map, makeWeightParams(k, 1));
    const DemandResult demand = makeDemand(map, weights, 1.0);
    // 08:00 早高峰的合理范围：2–6 分钟，而不是把全天平峰/深夜间隔混在一起。
    const int headways[] = {120, 180, 240, 300, 360};
    std::cout << "headway | R3 | R4 | evalMs\n";
    for (int headway : headways) {
        const EvalParams params =
            makeEvalParams(UnreachablePolicy::Exclude, headway, 30.0, 120.0);
        const Metrics metrics = evaluate(map, lines, weights, demand, params);
        std::cout << headway << " | " << number(metrics.r3) << " | "
                  << number(metrics.r4) << " | " << number(metrics.elapsedMs) << '\n';
    }
}

void printPlanLines(const Map& map, const DynamicArray<Line>& lines) {
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::cout << "  L" << i << ":";
        for (std::size_t j = 0; j < lines[i].stations().size(); ++j) {
            std::cout << ' ' << map.station(lines[i].stations()[j]).id;
        }
        std::cout << '\n';
    }
}

void printMergeExamples(const Map& map, const LineStructure& structure, int limit) {
    if (structure.mergeOpportunities.empty() || limit <= 0) {
        return;
    }
    DynamicArray<LineMergeOpportunity> sorted = structure.mergeOpportunities;
    quickSort(sorted, [](const LineMergeOpportunity& a,
                         const LineMergeOpportunity& b) {
        return a.turnDeg < b.turnDeg;
    });
    std::cout << "mergest candidates (straightest first):\n";
    for (std::size_t i = 0;
         i < sorted.size() && i < static_cast<std::size_t>(limit); ++i) {
        const LineMergeOpportunity& opportunity = sorted[i];
        std::cout << "  L" << opportunity.firstLine << " + L"
                  << opportunity.secondLine << " at "
                  << map.station(opportunity.junction).id
                  << " turn=" << number(opportunity.turnDeg) << "deg\n";
    }
}

void auditNetwork(const Network& network, int shortThreshold, int mergeExamples) {
    const LineStructure structure =
        auditLines(network.map(), network.lines(), shortThreshold);
    printLineStructure(structure);
    printMergeExamples(network.map(), structure, mergeExamples);
}

CoverageMode parseCoverageMode(const std::string& value) {
    if (value == "log") {
        return CoverageMode::MultiLineLog;
    }
    if (value == "sat") {
        return CoverageMode::MultiLineSaturating;
    }
    return CoverageMode::Unique;
}

void applyCoverageParams(EvalParams& params, const std::string& mode,
                         double lambda) {
    params.coverageMode = parseCoverageMode(mode);
    params.multiLineLambda = lambda;
}

void applySpeedMode(SearchParams& search, const std::string& mode, int stationCount) {
    std::string resolved = mode;
    if (resolved == "auto") {
        if (stationCount <= 40) {
            resolved = "deep";
        } else if (stationCount <= 100) {
            resolved = "full";
        } else {
            resolved = "balanced";
        }
    }
    if (resolved == "deep2") {
        search.timeBudgetMs = 120000;
        search.localSearchBudget = 1600;
        search.cleanupBudget = 1200;
    } else if (resolved == "deep1") {
        search.timeBudgetMs = 60000;
        search.localSearchBudget = 800;
        search.cleanupBudget = 600;
    } else if (resolved == "deep") {
        search.timeBudgetMs = 30000;
        search.localSearchBudget = 400;
        search.cleanupBudget = 300;
    } else if (resolved == "full") {
        search.timeBudgetMs = 0;
        search.localSearchBudget = 160;
        search.cleanupBudget = 120;
    } else if (resolved == "fast") {
        search.timeBudgetMs = 800;
        search.localSearchBudget = 50;
        search.cleanupBudget = 20;
    } else if (resolved == "turbo") {
        search.timeBudgetMs = 400;
        search.localSearchBudget = 20;
        search.cleanupBudget = 8;
    } else {  // balanced
        search.timeBudgetMs = 1500;
        search.localSearchBudget = 100;
        search.cleanupBudget = 60;
    }
}

void applyScoreProfile(ScoreParams& score, const std::string& profile) {
    if (profile == "coverage") {
        score.wCoverage = 0.50;
        score.wCost = 0.15;
        score.wTime = 0.15;
        score.wTransfer = 0.05;
        score.wDetour = 0.05;
        score.wShort = 0.10;
    } else if (profile == "service") {
        score.wCoverage = 0.20;
        score.wCost = 0.10;
        score.wTime = 0.35;
        score.wTransfer = 0.20;
        score.wDetour = 0.05;
        score.wShort = 0.10;
    }
}

void modePlan(int cols, int rows, int k, int maxLines, const std::string& profile,
              double minCoverage, double coveragePenalty, const std::string& scenario,
              int cityCenters, int perCenter, int citySeed, bool localSearch,
              int localBudget, bool prefilter, const std::string& r1Mode,
              double coverageLambda, bool corridorCandidates,
              const std::string& speedMode, const std::string& outMapPath,
              const std::string& outLinesPath) {
    DynamicArray<Line> baselineLines;
    const Map map = scenario == "grid"
                        ? makeGridMap(cols, rows)
                        : makeSimulatedCity(cityCenters, perCenter, 2.6, citySeed,
                                            &baselineLines);
    const WeightResult weights = computeWeights(map, makeWeightParams(k, 1));
    const DemandResult demand = makeDemand(map, weights, 1.0);
    EvalParams params = makeEvalParams(UnreachablePolicy::Penalty, 300, 30.0, 120.0);
    params.connectivity = ConnectivityRequirement::CoveredStations;
    applyCoverageParams(params, r1Mode, coverageLambda);
    SearchParams search;
    search.maxLines = maxLines;
    search.topStations = std::min(16, map.stationCount());
    search.candidateLimit = scenario == "grid" ? 120 : 320;
    search.localSearchBudget = localBudget > 0 ? localBudget : (scenario == "grid" ? 120 : 24);
    search.enableLocalSearch = localSearch;
    search.useCorridorCandidates = corridorCandidates;
    applySpeedMode(search, speedMode, map.stationCount());
    search.prefilter = prefilter;
    applyScoreProfile(search.score, profile);
    search.score.minCoverage = minCoverage;
    search.score.coveragePenalty = coveragePenalty;
    std::cout << "scenario=" << scenario << " stations=" << map.stationCount()
              << " edges=" << map.edges().size() << " r1=" << r1Mode
              << " lambda=" << number(coverageLambda) << '\n';

    const PlanResult plan = buildGreedyPlan(map, weights, demand, params, search);
    std::cout << "plan: " << map.stationCount() << " stations, "
              << plan.lines.size() << " lines, steps=" << plan.steps
              << " localMoves=" << plan.localMoves
              << " merged=" << plan.mergedLines
              << " duplicatesRemoved=" << plan.duplicateLinesRemoved
              << " redundantRemoved=" << plan.redundantLinesRemoved
              << " evaluations=" << plan.candidatesEvaluated
              << " score=" << number(plan.score) << '\n';
    printPlanLines(map, plan.lines);
    std::cout << "greedy base:\n";
    printMetrics(plan.baseMetrics);
    std::cout << "after local search:\n";
    printMetrics(plan.metrics);
    const LineStructure structure = auditLines(map, plan.lines, 3);
    printLineStructure(structure);
    printMergeExamples(map, structure, 8);

    if (scenario == "grid") {
        const DynamicArray<Line> gridLines = makeGridLines(map, cols, rows, true, true);
        const Metrics gridMetrics = evaluate(map, gridLines, weights, demand, params);
        std::cout << "full grid baseline: " << gridLines.size() << " lines\n";
        printMetrics(gridMetrics);
    } else if (!baselineLines.empty()) {
        const Metrics baselineMetrics =
            evaluate(map, baselineLines, weights, demand, params);
        std::cout << "radial spoke baseline: " << baselineLines.size() << " lines\n";
        printMetrics(baselineMetrics);
    }

    if (!outMapPath.empty() && !outLinesPath.empty()) {
        Network output;
        output.map() = map;
        for (std::size_t i = 0; i < plan.lines.size(); ++i) {
            Line line = plan.lines[i];
            line.setName("Planned " + std::to_string(i + 1));
            line.ops().dwellSec = 30.0;
            line.ops().headway.clear();
            line.ops().headway.push_back(HeadwayPeriod{0, 86400, 300});
            line.ops().segmentRunSec.clear();
            for (int j = 0; j + 1 < line.stationCount(); ++j) {
                line.ops().segmentRunSec.push_back(120.0);
            }
            output.addLine(static_cast<Line&&>(line));
        }
        std::string error;
        if (!io::saveMap(outMapPath, map, &error)) {
            std::cout << "map save failed: " << error << '\n';
        } else if (!io::saveLines(outLinesPath, output, &error)) {
            std::cout << "line save failed: " << error << '\n';
        } else {
            std::cout << "saved map=" << outMapPath << " lines=" << outLinesPath << '\n';
        }
    }
}

void modePlanFile(const std::string& mapPath, const std::string& outLinesPath,
                  int k, int maxLines, int localBudget, bool prefilter,
                  const std::string& r1Mode, double coverageLambda,
                  double lineOverlap, double lineProximity, double proximityDistance,
                  int minLineStations, bool corridorCandidates, bool corridorOnly,
                  double interiorConnector, double corridorEdgeRatio,
                  const std::string& speedMode, int cleanupBudget,
                  int timeBudgetOverride) {
    Network source;
    std::string error;
    if (!io::loadMap(mapPath, source.map(), &error)) {
        std::cout << "map load failed: " << error << '\n';
        return;
    }
    const WeightResult weights = computeWeights(source.map(), makeWeightParams(k, 1));
    const DemandResult demand = makeDemand(source.map(), weights, 1.0);
    EvalParams params = makeEvalParams(UnreachablePolicy::Penalty, 300, 30.0, 120.0);
    params.connectivity = ConnectivityRequirement::CoveredStations;
    applyCoverageParams(params, r1Mode, coverageLambda);

    SearchParams search;
    search.maxLines = maxLines;
    // 规模自适应：大一点的地图不能只靠少量锚点，否则外围站进不了候选池。
    const int stationCount = source.map().stationCount();
    search.topStations = std::min(24, stationCount);
    search.extraAnchors = std::min(24, std::max(12, stationCount / 3));
    search.candidateLimit = std::max(600, stationCount * 12);
    search.localSearchBudget = localBudget > 0 ? localBudget : 160;
    search.prefilter = prefilter;
    search.score.wLineOverlap = lineOverlap;
    search.score.wLineProximity = lineProximity;
    search.score.lineProximityDistance = proximityDistance;
    search.score.wInteriorConnector = interiorConnector;
    search.corridorEdgeRatio = corridorEdgeRatio;
    search.minLineStations = minLineStations;
    search.useCorridorCandidates = corridorCandidates;
    applySpeedMode(search, speedMode, stationCount);
    if (timeBudgetOverride >= 0) {
        search.timeBudgetMs = timeBudgetOverride;
    }
    if (localBudget > 0) {
        search.localSearchBudget = localBudget;
    }
    if (cleanupBudget > 0) {
        search.cleanupBudget = cleanupBudget;
    }
    search.corridorOnly = corridorOnly;

    const PlanResult plan =
        buildGreedyPlan(source.map(), weights, demand, params, search);
    Network output;
    output.map() = source.map();
    for (std::size_t i = 0; i < plan.lines.size(); ++i) {
        Line line = plan.lines[i];
        line.setName("Planned " + std::to_string(i + 1));
        line.ops().dwellSec = 30.0;
        line.ops().headway.clear();
        line.ops().headway.push_back(HeadwayPeriod{0, 86400, 300});
        line.ops().segmentRunSec.clear();
        for (int j = 0; j + 1 < line.stationCount(); ++j) {
            line.ops().segmentRunSec.push_back(120.0);
        }
        output.addLine(static_cast<Line&&>(line));
    }
    if (!io::saveLines(outLinesPath, output, &error)) {
        std::cout << "line save failed: " << error << '\n';
        return;
    }
    std::cout << "map=" << mapPath << " stations=" << source.map().stationCount()
              << " edges=" << source.map().edges().size() << " r1=" << r1Mode
              << " lambda=" << number(coverageLambda) << '\n';
    std::cout << "planned lines=" << plan.lines.size()
              << " planMs=" << number(plan.elapsedMs)
              << " merged=" << plan.mergedLines
              << " duplicatesRemoved=" << plan.duplicateLinesRemoved
              << " redundantRemoved=" << plan.redundantLinesRemoved
              << " output=" << outLinesPath << '\n';
    printMetrics(plan.metrics);
    auditNetwork(output, 3, 8);
}

void modeCorridors(const std::string& mapPath, const std::string& linePath,
                   double corridorEdgeRatio) {
    Map map;
    std::string error;
    if (!io::loadMap(mapPath, map, &error)) {
        std::cout << "map load failed: " << error << '\n';
        return;
    }
    std::cout << "corridors: " << mapPath << '\n';
    const WeightResult weights = computeWeights(map, makeWeightParams(3, 1));
    const DemandResult demand = makeDemand(map, weights, 1.0);
    CorridorOptions options;
    options.corridorEdgeRatio = corridorEdgeRatio;
    const CorridorGraph graph = buildStationCorridorGraph(map, weights, demand, options);
    printCorridorGraph(graph);
    if (!linePath.empty()) {
        Network network;
        network.map() = map;
        if (!io::loadLines(linePath, network, &error)) {
            std::cout << "line load failed: " << error << '\n';
            return;
        }
        printCorridorLineUsage(corridorLineUsage(map, graph, network.lines()));
    }
}

void modeCheckMap(const std::string& mapPath) {
    Map map;
    std::string error;
    if (!io::loadMap(mapPath, map, &error)) {
        std::cout << "map load failed: " << error << '\n';
        return;
    }
    std::cout << "check-map: " << mapPath << '\n';
    printMapGeometry(map, auditMapGeometry(map));
    printNearestEdges(auditNearestEdges(map));
}

void modeAudit(const std::string& mapPath, const std::string& linePath,
               int shortThreshold) {
    Network network;
    std::string error;
    if (!io::loadMap(mapPath, network.map(), &error)) {
        std::cout << "map load failed: " << error << '\n';
        return;
    }
    if (!io::loadLines(linePath, network, &error)) {
        std::cout << "line load failed: " << error << '\n';
        return;
    }
    std::cout << "audit: map=" << mapPath << " lines=" << linePath << '\n';
    auditNetwork(network, shortThreshold, 16);
}

void modeDemo(const std::string& mapPath, const std::string& linePath) {
    Network network;
    std::string error;
    if (!io::loadMap(mapPath, network.map(), &error)) {
        std::cout << "map load failed: " << error << '\n';
        return;
    }
    if (!io::loadLines(linePath, network, &error)) {
        std::cout << "line load failed: " << error << '\n';
        return;
    }
    const WeightResult weights = computeWeights(network.map(), makeWeightParams(3, 1));
    const DemandResult demand = makeDemand(network.map(), weights, 1.0);
    EvalParams params = makeEvalParams(UnreachablePolicy::Exclude, 300, 30.0, 120.0);
    params.overrideLineOps = false;
    const Metrics metrics = evaluateNetwork(network, weights, demand, params);
    std::cout << "demo network: " << network.map().stationCount() << " stations, "
              << network.lines().size() << " lines\n";
    printMetrics(metrics);
}

void usage() {
    std::cout << "plan_lab <mode> [options]\n"
              << "  weights      --cols 8 --rows 8 --k 3 --seed 1\n"
              << "  stability    --cols 10 --rows 10 --k 3\n"
              << "  stability-clustered --per 50 --clusters 3 --k 3\n"
              << "  timing       --k 3\n"
              << "  policy       --cols 8 --rows 8 --k 3\n"
              << "  ops          --cols 8 --rows 8 --k 3\n"
              << "  plan         --scenario city|grid --centers 1 --per-center 60 --city-seed 1\n"
              << "               --k 3 --max-lines 8 --profile balanced|coverage|service\n"
              << "               --min-coverage 0.8 --coverage-penalty 1.0 [--no-local]\n"
              << "               [--local-budget N] [--cleanup-budget N] [--time-budget-ms N] [--no-prefilter]\n"
              << "               [--r1-mode sat|log|unique] [--r1-lambda 0.25]\n"
              << "               default r1-mode=sat lambda=0.25\n"
              << "  plan-file    --map data/city_demo.cuc4m --out-lines data/city_demo_planned.cuc3l\n"
              << "  audit        --map data/city_demo.cuc4m --lines data/city_demo.cuc3l [--short-threshold 3]\n"
              << "  check-map    --map data/city_demo.cuc4m\n"
              << "  corridors    --map data/city60.cuc4m\n"
              << "  demo         --map data/demo.cuc4m --lines data/demo.cuc3l\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }
    const std::string mode = argv[1];
    int cols = 8;
    int rows = 8;
    int k = 3;
    int seed = 1;
    int perCluster = 50;
    int spatialClusters = 3;
    int maxLines = 8;
    std::string profile = "balanced";
    std::string scenario = "city";
    int cityCenters = 1;
    int perCenter = 60;
    int citySeed = 1;
    double minCoverage = 0.8;
    double coveragePenalty = 1.0;
    bool localSearch = true;
    bool prefilter = true;
    int localBudget = 0;
    int cleanupBudget = 0;
    int timeBudgetOverride = -1;
    int shortThreshold = 3;
    std::string r1Mode = "sat";
    double coverageLambda = 0.25;
    double lineOverlap = 0.10;
    double lineProximity = 0.03;
    double proximityDistance = 7.0;
    double interiorConnector = 0.0;
    double corridorEdgeRatio = 0.10;
    int minLineStations = 2;
    bool corridorCandidates = true;
    bool corridorOnly = false;
    std::string speedMode = "balanced";
    std::string mapPath = "data/demo.cuc4m";
    std::string linePath = "data/demo.cuc3l";
    std::string outMapPath = "data/city_planned.cuc4m";
    std::string outLinesPath = "data/city_demo_planned.cuc3l";

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--cols" && i + 1 < argc) {
            cols = std::stoi(argv[++i]);
        } else if (arg == "--rows" && i + 1 < argc) {
            rows = std::stoi(argv[++i]);
        } else if (arg == "--k" && i + 1 < argc) {
            k = std::stoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = std::stoi(argv[++i]);
        } else if (arg == "--per" && i + 1 < argc) {
            perCluster = std::stoi(argv[++i]);
        } else if (arg == "--clusters" && i + 1 < argc) {
            spatialClusters = std::stoi(argv[++i]);
        } else if (arg == "--max-lines" && i + 1 < argc) {
            maxLines = std::stoi(argv[++i]);
        } else if (arg == "--profile" && i + 1 < argc) {
            profile = argv[++i];
        } else if (arg == "--scenario" && i + 1 < argc) {
            scenario = argv[++i];
        } else if (arg == "--centers" && i + 1 < argc) {
            cityCenters = std::stoi(argv[++i]);
        } else if (arg == "--per-center" && i + 1 < argc) {
            perCenter = std::stoi(argv[++i]);
        } else if (arg == "--city-seed" && i + 1 < argc) {
            citySeed = std::stoi(argv[++i]);
        } else if (arg == "--min-coverage" && i + 1 < argc) {
            minCoverage = std::stod(argv[++i]);
        } else if (arg == "--coverage-penalty" && i + 1 < argc) {
            coveragePenalty = std::stod(argv[++i]);
        } else if (arg == "--no-local") {
            localSearch = false;
        } else if (arg == "--no-prefilter") {
            prefilter = false;
        } else if (arg == "--local-budget" && i + 1 < argc) {
            localBudget = std::stoi(argv[++i]);
        } else if (arg == "--cleanup-budget" && i + 1 < argc) {
            cleanupBudget = std::stoi(argv[++i]);
        } else if (arg == "--time-budget-ms" && i + 1 < argc) {
            timeBudgetOverride = std::stoi(argv[++i]);
        } else if (arg == "--short-threshold" && i + 1 < argc) {
            shortThreshold = std::stoi(argv[++i]);
        } else if (arg == "--r1-mode" && i + 1 < argc) {
            r1Mode = argv[++i];
        } else if (arg == "--r1-lambda" && i + 1 < argc) {
            coverageLambda = std::stod(argv[++i]);
        } else if (arg == "--line-overlap" && i + 1 < argc) {
            lineOverlap = std::stod(argv[++i]);
        } else if (arg == "--line-proximity" && i + 1 < argc) {
            lineProximity = std::stod(argv[++i]);
        } else if (arg == "--proximity-distance" && i + 1 < argc) {
            proximityDistance = std::stod(argv[++i]);
        } else if (arg == "--min-line-stations" && i + 1 < argc) {
            minLineStations = std::stoi(argv[++i]);
        } else if (arg == "--interior-connector" && i + 1 < argc) {
            interiorConnector = std::stod(argv[++i]);
        } else if (arg == "--corridor-edge-ratio" && i + 1 < argc) {
            corridorEdgeRatio = std::stod(argv[++i]);
        } else if (arg == "--corridor-candidates") {
            corridorCandidates = true;
        } else if (arg == "--no-corridor-candidates") {
            corridorCandidates = false;
        } else if (arg == "--speed" && i + 1 < argc) {
            speedMode = argv[++i];
        } else if (arg == "--corridor-only") {
            corridorCandidates = true;
            corridorOnly = true;
        } else if (arg == "--map" && i + 1 < argc) {
            mapPath = argv[++i];
        } else if (arg == "--lines" && i + 1 < argc) {
            linePath = argv[++i];
        } else if (arg == "--out-lines" && i + 1 < argc) {
            outLinesPath = argv[++i];
        } else if (arg == "--out-map" && i + 1 < argc) {
            outMapPath = argv[++i];
        }
    }

    if (mode == "weights") {
        modeWeights(cols, rows, k, seed);
    } else if (mode == "stability") {
        modeWeightStability(cols, rows, k);
    } else if (mode == "stability-clustered") {
        modeClusteredStability(perCluster, spatialClusters, k);
    } else if (mode == "timing") {
        modeTiming(k);
    } else if (mode == "policy") {
        modePolicy(cols, rows, k);
    } else if (mode == "ops") {
        modeOps(cols, rows, k);
    } else if (mode == "plan") {
        modePlan(cols, rows, k, maxLines, profile, minCoverage, coveragePenalty,
                 scenario, cityCenters, perCenter, citySeed, localSearch, localBudget,
                 prefilter, r1Mode, coverageLambda, corridorCandidates, speedMode,
                 outMapPath, outLinesPath);
    } else if (mode == "plan-file") {
        modePlanFile(mapPath, outLinesPath, k, maxLines, localBudget, prefilter,
                     r1Mode, coverageLambda, lineOverlap, lineProximity,
                     proximityDistance, minLineStations, corridorCandidates, corridorOnly,
                     interiorConnector, corridorEdgeRatio, speedMode, cleanupBudget,
                     timeBudgetOverride);
    } else if (mode == "audit") {
        modeAudit(mapPath, linePath, shortThreshold);
    } else if (mode == "check-map") {
        modeCheckMap(mapPath);
    } else if (mode == "corridors") {
        modeCorridors(mapPath, linePath, corridorEdgeRatio);
    } else if (mode == "demo") {
        modeDemo(mapPath, linePath);
    } else {
        usage();
        return 1;
    }
    return 0;
}
