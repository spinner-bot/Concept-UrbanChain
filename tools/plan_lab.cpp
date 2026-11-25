#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include "core/map.h"
#include "core/network.h"
#include "ds/sort.h"
#include "io/line_io.h"
#include "io/map_io.h"
#include "plan/demand.h"
#include "plan/evaluator.h"
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
              << "  demo         --map data/demo.cuc3m3 --lines data/demo.cuc3l\n";
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
    std::string mapPath = "data/demo.cuc3m3";
    std::string linePath = "data/demo.cuc3l";

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
        } else if (arg == "--map" && i + 1 < argc) {
            mapPath = argv[++i];
        } else if (arg == "--lines" && i + 1 < argc) {
            linePath = argv[++i];
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
    } else if (mode == "demo") {
        modeDemo(mapPath, linePath);
    } else {
        usage();
        return 1;
    }
    return 0;
}
