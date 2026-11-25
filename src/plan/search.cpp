#include "plan/search.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include "ds/binary_heap.h"
#include "ds/hash_map.h"
#include "ds/sort.h"
#include "plan/evaluator.h"

namespace uc {
namespace plan {
namespace {

DynamicArray<Index> shortestPath(const Map& map, Index start, Index goal) {
    DynamicArray<Index> path;
    if (start == goal || start < 0 || goal < 0 || start >= map.stationCount() ||
        goal >= map.stationCount()) {
        return path;
    }

    const int count = map.stationCount();
    DynamicArray<double> dist;
    DynamicArray<int> previous;
    dist.resize(static_cast<std::size_t>(count));
    previous.resize(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        dist[static_cast<std::size_t>(i)] = kInfinity;
        previous[static_cast<std::size_t>(i)] = -1;
    }

    BinaryMinHeap heap;
    dist[static_cast<std::size_t>(start)] = 0.0;
    heap.push(0.0, start);
    while (!heap.empty()) {
        const HeapItem item = heap.pop();
        const Index station = item.value;
        if (item.key > dist[static_cast<std::size_t>(station)] + 1e-9) {
            continue;
        }
        if (station == goal) {
            break;
        }
        const DynamicArray<AdjEdge>& neighbors = map.neighbors(station);
        for (std::size_t i = 0; i < neighbors.size(); ++i) {
            const AdjEdge& edge = neighbors[i];
            const double next = item.key + edge.cost;
            if (next + 1e-9 < dist[static_cast<std::size_t>(edge.to)]) {
                dist[static_cast<std::size_t>(edge.to)] = next;
                previous[static_cast<std::size_t>(edge.to)] = station;
                heap.push(next, edge.to);
            }
        }
    }

    if (std::isinf(dist[static_cast<std::size_t>(goal)])) {
        return path;
    }
    int current = goal;
    while (current >= 0) {
        path.push_back(current);
        if (current == start) {
            break;
        }
        current = previous[static_cast<std::size_t>(current)];
    }
    if (path.empty() || path.back() != start) {
        path.clear();
        return path;
    }
    for (std::size_t i = 0, j = path.size(); i + 1 < j; ++i, --j) {
        const Index tmp = path[i];
        path[i] = path[j - 1];
        path[j - 1] = tmp;
    }
    return path;
}

std::string pathKey(const DynamicArray<Index>& path) {
    std::string forward;
    std::string backward;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i > 0) {
            forward += '-';
        }
        forward += std::to_string(path[i]);
    }
    for (std::size_t i = path.size(); i > 0; --i) {
        if (i < path.size()) {
            backward += '-';
        }
        backward += std::to_string(path[i - 1]);
    }
    return forward < backward ? forward : backward;
}

Line makeCandidate(const DynamicArray<Index>& path, std::uint32_t color) {
    Line line(0, "candidate", color);
    for (std::size_t i = 0; i < path.size(); ++i) {
        line.stations().push_back(path[i]);
    }
    line.ops().dwellSec = 30.0;
    line.ops().headway.push_back(HeadwayPeriod{0, 86400, 300});
    for (std::size_t i = 1; i < path.size(); ++i) {
        line.ops().segmentRunSec.push_back(120.0);
    }
    return line;
}

}  // namespace

PlanResult buildGreedyPlan(const Map& map, const WeightResult& weights,
                           const DemandResult& demand, const EvalParams& params,
                           const SearchParams& search) {
    PlanResult result;
    const int stationCount = map.stationCount();
    if (stationCount < 2 || static_cast<int>(weights.weights.size()) != stationCount) {
        return result;
    }

    const std::uint32_t palette[] = {0xef4444u, 0x2563ebu, 0x16a34au, 0xf59e0bu,
                                     0x7c3aedu, 0x0891b2u, 0xdb2777u, 0x65a30du};
    DynamicArray<Line> candidates;
    HashMap<std::string, unsigned char, Fnv1aHash> seen;

    auto addCandidate = [&](Index a, Index b) {
        if (static_cast<int>(candidates.size()) >= search.candidateLimit) {
            return;
        }
        const DynamicArray<Index> path = shortestPath(map, a, b);
        if (path.size() < 2) {
            return;
        }
        const std::string key = pathKey(path);
        if (seen.contains(key)) {
            return;
        }
        seen.insert(key, 1);
        candidates.push_back(makeCandidate(path, palette[candidates.size() % 8]));
    };

    DynamicArray<int> top;
    for (int i = 0; i < stationCount; ++i) {
        top.push_back(i);
    }
    quickSort(top, [&weights](int a, int b) {
        return weights.weights[static_cast<std::size_t>(a)] >
               weights.weights[static_cast<std::size_t>(b)];
    });
    const int topCount = std::max(2, std::min(search.topStations, stationCount));
    for (int i = 0; i < topCount; ++i) {
        for (int j = i + 1; j < topCount; ++j) {
            addCandidate(top[static_cast<std::size_t>(i)], top[static_cast<std::size_t>(j)]);
        }
    }

    struct OdPair {
        int a = 0;
        int b = 0;
        double od = 0.0;
    };
    DynamicArray<OdPair> pairs;
    const int demandCount = demand.stationCount;
    if (demandCount == stationCount) {
        for (int i = 0; i < stationCount; ++i) {
            for (int j = i + 1; j < stationCount; ++j) {
                const double od =
                    demand.od[static_cast<std::size_t>(i) * stationCount +
                              static_cast<std::size_t>(j)];
                if (od > 0.0) {
                    pairs.push_back(OdPair{i, j, od});
                }
            }
        }
        quickSort(pairs, [](const OdPair& a, const OdPair& b) { return a.od > b.od; });
        const int demandCandidates = std::min(static_cast<int>(pairs.size()),
                                              search.topStations * 4);
        for (int i = 0; i < demandCandidates; ++i) {
            addCandidate(pairs[static_cast<std::size_t>(i)].a,
                         pairs[static_cast<std::size_t>(i)].b);
        }
    }

    EvalParams staticParams = params;
    staticParams.evaluateOd = false;
    staticParams.overrideLineOps = true;
    staticParams.connectivity = ConnectivityRequirement::CoveredStations;

    DynamicArray<Line> current;
    DynamicArray<unsigned char> covered;
    covered.resize(static_cast<std::size_t>(stationCount));
    for (int i = 0; i < stationCount; ++i) {
        covered[static_cast<std::size_t>(i)] = 0;
    }
    DynamicArray<unsigned char> used;
    used.resize(candidates.size());
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        used[i] = 0;
    }

    double currentR1 = 0.0;
    double currentR2 = 0.0;
    for (int step = 0; step < search.maxLines; ++step) {
        int best = -1;
        double bestScore = -kInfinity;
        double bestGain = 0.0;
        double bestR1 = 0.0;
        double bestR2 = 0.0;
        for (std::size_t c = 0; c < candidates.size(); ++c) {
            if (used[c]) {
                continue;
            }
            const Line& candidate = candidates[c];
            if (!current.empty()) {
                bool shares = false;
                for (std::size_t i = 0; i < candidate.stations().size(); ++i) {
                    if (covered[static_cast<std::size_t>(candidate.stations()[i])]) {
                        shares = true;
                        break;
                    }
                }
                if (!shares) {
                    continue;
                }
            }

            DynamicArray<Line> trial = current;
            trial.push_back(candidate);
            ++result.candidatesEvaluated;
            const Metrics metrics = evaluate(map, trial, weights, demand, staticParams);
            if (!metrics.feasible) {
                continue;
            }
            const double gain = metrics.r1 - currentR1;
            const double cost = std::max(0.0, metrics.r2 - currentR2);
            if (gain <= search.minCoverageGain) {
                continue;
            }
            const double score = gain / std::max(cost, 1e-9);
            if (score > bestScore + 1e-12 ||
                (std::fabs(score - bestScore) <= 1e-12 && gain > bestGain)) {
                best = static_cast<int>(c);
                bestScore = score;
                bestGain = gain;
                bestR1 = metrics.r1;
                bestR2 = metrics.r2;
            }
        }
        if (best < 0) {
            break;
        }
        const Line& chosen = candidates[static_cast<std::size_t>(best)];
        current.push_back(chosen);
        used[static_cast<std::size_t>(best)] = 1;
        for (std::size_t i = 0; i < chosen.stations().size(); ++i) {
            covered[static_cast<std::size_t>(chosen.stations()[i])] = 1;
        }
        currentR1 = bestR1;
        currentR2 = bestR2;
        ++result.steps;
    }

    EvalParams finalParams = params;
    finalParams.evaluateOd = true;
    finalParams.overrideLineOps = true;
    finalParams.connectivity = ConnectivityRequirement::CoveredStations;
    result.lines = current;
    result.metrics = evaluate(map, current, weights, demand, finalParams);
    return result;
}

}  // namespace plan
}  // namespace uc
