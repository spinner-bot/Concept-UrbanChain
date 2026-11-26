#include "plan/search.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include "ds/binary_heap.h"
#include "ds/hash_map.h"
#include "ds/sort.h"
#include "plan/audit.h"
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
            const double next = item.key + edge.unitCost * edge.length;
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

DynamicArray<Index> shortestPathAvoiding(const Map& map, Index start, Index goal,
                                         const DynamicArray<unsigned char>& forbidden) {
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
            if (forbidden[static_cast<std::size_t>(edge.to)] && edge.to != goal) {
                continue;
            }
            const double next = item.key + edge.unitCost * edge.length;
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

bool sameStationSet(const Line& a, const Line& b) {
    if (a.stationCount() != b.stationCount()) {
        return false;
    }
    for (int i = 0; i < a.stationCount(); ++i) {
        bool found = false;
        for (int j = 0; j < b.stationCount(); ++j) {
            if (a.stations()[static_cast<std::size_t>(i)] ==
                b.stations()[static_cast<std::size_t>(j)]) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

bool isStationSubset(const Line& candidate, const Line& container) {
    if (candidate.stationCount() >= container.stationCount()) {
        return false;
    }
    for (int i = 0; i < candidate.stationCount(); ++i) {
        if (!container.containsStation(candidate.stations()[static_cast<std::size_t>(i)])) {
            return false;
        }
    }
    return true;
}

bool containsDuplicateLine(const DynamicArray<Line>& lines, const Line& candidate,
                           int ignore = -1) {
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (static_cast<int>(i) == ignore) {
            continue;
        }
        if (sameStationSet(lines[i], candidate)) {
            return true;
        }
    }
    return false;
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

double scoreMetrics(const Metrics& metrics, const ScoreParams& score) {
    if (!metrics.feasible || std::isinf(metrics.r3) || std::isinf(metrics.r4)) {
        return -kInfinity;
    }
    double value =
        score.wCoverage * metrics.r1 -
        score.wCost * metrics.r2 / std::max(score.refCost, 1e-9) -
        score.wTime * metrics.r3 / std::max(score.refTime, 1e-9) -
        score.wTransfer * metrics.r4 / std::max(score.refTransfer, 1e-9) -
        score.wDetour * metrics.r5 / std::max(score.refDetour, 1e-9) -
        score.wShort * metrics.r6 / std::max(score.refShort, 1e-9);
    const double deficit = score.minCoverage - metrics.r1;
    if (deficit > 0.0) {
        value -= score.coveragePenalty * deficit * deficit;
    }
    return value;
}

double lineSimilarityPenaltyInternal(const Map& map, const DynamicArray<Line>& lines,
                                     const ScoreParams& score) {
    if (lines.size() < 2) {
        return 0.0;
    }
    double overlapSum = 0.0;
    double proximitySum = 0.0;
    int pairs = 0;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const Line& a = lines[i];
        if (a.stationCount() < 2) {
            continue;
        }
        for (std::size_t j = i + 1; j < lines.size(); ++j) {
            const Line& b = lines[j];
            if (b.stationCount() < 2) {
                continue;
            }
            ++pairs;

            // 共线程度：两条线路共享的区间数 / 较短线路的区间数。
            int sharedEdges = 0;
            for (int ea = 0; ea + 1 < a.stationCount(); ++ea) {
                const Index a0 = a.stations()[static_cast<std::size_t>(ea)];
                const Index a1 = a.stations()[static_cast<std::size_t>(ea + 1)];
                for (int eb = 0; eb + 1 < b.stationCount(); ++eb) {
                    const Index b0 = b.stations()[static_cast<std::size_t>(eb)];
                    const Index b1 = b.stations()[static_cast<std::size_t>(eb + 1)];
                    if ((a0 == b0 && a1 == b1) || (a0 == b1 && a1 == b0)) {
                        ++sharedEdges;
                        break;
                    }
                }
            }
            const int shorterEdges = std::min(a.stationCount() - 1, b.stationCount() - 1);
            if (shorterEdges > 0) {
                overlapSum += static_cast<double>(sharedEdges) / shorterEdges;
            }

            // 过近程度：一条线路的站点中，有多少落在另一条线路站点的
            // 近邻半径内；双向平均，避免短线路单方面被长线路支配。
            auto nearFraction = [&](const Line& source, const Line& target) {
                if (source.stationCount() <= 0) {
                    return 0.0;
                }
                int near = 0;
                for (int s = 0; s < source.stationCount(); ++s) {
                    const Vec2& point =
                        map.station(source.stations()[static_cast<std::size_t>(s)]).position;
                    double best = kInfinity;
                    for (int t = 0; t < target.stationCount(); ++t) {
                        best = std::min(
                            best,
                            euclideanDistance(
                                point,
                                map.station(target.stations()[static_cast<std::size_t>(t)])
                                    .position));
                    }
                    if (best <= score.lineProximityDistance) {
                        ++near;
                    }
                }
                return static_cast<double>(near) / source.stationCount();
            };
            proximitySum += 0.5 * (nearFraction(a, b) + nearFraction(b, a));
        }
    }
    if (pairs <= 0) {
        return 0.0;
    }
    return score.wLineOverlap * overlapSum / pairs +
           score.wLineProximity * proximitySum / pairs;
}

double scorePlan(const Map& map, const DynamicArray<Line>& lines, const Metrics& metrics,
                 const ScoreParams& score) {
    return scoreMetrics(metrics, score) - lineSimilarityPenaltyInternal(map, lines, score);
}

void recomputeCovered(const Map& map, const DynamicArray<Line>& lines,
                      DynamicArray<unsigned char>& covered) {
    for (int i = 0; i < map.stationCount(); ++i) {
        covered[static_cast<std::size_t>(i)] = 0;
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
        for (std::size_t j = 0; j < lines[i].stations().size(); ++j) {
            covered[static_cast<std::size_t>(lines[i].stations()[j])] = 1;
        }
    }
}

}  // namespace

double lineSimilarityPenalty(const Map& map, const DynamicArray<Line>& lines,
                             const ScoreParams& score) {
    return lineSimilarityPenaltyInternal(map, lines, score);
}

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
        if (static_cast<int>(path.size()) < std::max(2, search.minLineStations)) {
            return;
        }
        const std::string key = pathKey(path);
        if (seen.contains(key)) {
            return;
        }
        seen.insert(key, 1);
        candidates.push_back(makeCandidate(path, palette[candidates.size() % 8]));
    };

    DynamicArray<int> anchors;
    auto addAnchor = [&anchors, stationCount](int index) {
        if (index < 0 || index >= stationCount) {
            return;
        }
        for (std::size_t i = 0; i < anchors.size(); ++i) {
            if (anchors[i] == index) {
                return;
            }
        }
        anchors.push_back(index);
    };

    DynamicArray<int> order;
    for (int i = 0; i < stationCount; ++i) {
        order.push_back(i);
    }
    quickSort(order, [&weights](int a, int b) {
        return weights.weights[static_cast<std::size_t>(a)] >
               weights.weights[static_cast<std::size_t>(b)];
    });
    const int topCount = std::max(2, std::min(search.topStations, stationCount));
    for (int i = 0; i < topCount; ++i) {
        addAnchor(order[static_cast<std::size_t>(i)]);
    }

    // 额外锚点：从全局中心出发做最远点采样，保证外围站点进入候选池。
    if (search.extraAnchors > 0) {
        int first = 0;
        double farthest = -1.0;
        for (int i = 0; i < stationCount; ++i) {
            const double d =
                euclideanDistance(map.station(i).position, weights.globalCenter);
            if (d > farthest) {
                farthest = d;
                first = i;
            }
        }
        addAnchor(first);
        for (int extra = 0; extra < search.extraAnchors; ++extra) {
            int chosen = -1;
            double chosenDistance = -1.0;
            for (int i = 0; i < stationCount; ++i) {
                double nearest = kInfinity;
                for (std::size_t a = 0; a < anchors.size(); ++a) {
                    nearest = std::min(
                        nearest,
                        euclideanDistance(map.station(i).position,
                                          map.station(anchors[a]).position));
                }
                if (nearest > chosenDistance) {
                    chosenDistance = nearest;
                    chosen = i;
                }
            }
            const std::size_t before = anchors.size();
            addAnchor(chosen);
            if (anchors.size() == before) {
                break;
            }
        }
    }

    // 方向多样性种子：先放入跨城/跨轴的候选，避免锚点对排序把候选池
    // 全部塞满同一方向。网格图中这会显式提供南北向候选。
    auto addExtremePair = [&](Index a, Index b) {
        if (a >= 0 && b >= 0 && a != b) {
            addCandidate(a, b);
        }
    };
    int minX = -1;
    int maxX = -1;
    int minY = -1;
    int maxY = -1;
    int minSum = -1;
    int maxSum = -1;
    int minDiff = -1;
    int maxDiff = -1;
    for (int i = 0; i < stationCount; ++i) {
        const Vec2 p = map.station(i).position;
        if (minX < 0 || p.x < map.station(minX).position.x) minX = i;
        if (maxX < 0 || p.x > map.station(maxX).position.x) maxX = i;
        if (minY < 0 || p.y < map.station(minY).position.y) minY = i;
        if (maxY < 0 || p.y > map.station(maxY).position.y) maxY = i;
        if (minSum < 0 || p.x + p.y < map.station(minSum).position.x +
                                           map.station(minSum).position.y)
            minSum = i;
        if (maxSum < 0 || p.x + p.y > map.station(maxSum).position.x +
                                           map.station(maxSum).position.y)
            maxSum = i;
        if (minDiff < 0 || p.x - p.y < map.station(minDiff).position.x -
                                           map.station(minDiff).position.y)
            minDiff = i;
        if (maxDiff < 0 || p.x - p.y > map.station(maxDiff).position.x -
                                           map.station(maxDiff).position.y)
            maxDiff = i;
    }
    addExtremePair(minX, maxX);
    addExtremePair(minY, maxY);
    addExtremePair(minSum, maxSum);
    addExtremePair(minDiff, maxDiff);

    constexpr int kSectors = 8;
    constexpr double kTwoPi = 6.28318530717958647692;
    int sectorStation[kSectors];
    double sectorDistance[kSectors];
    for (int s = 0; s < kSectors; ++s) {
        sectorStation[s] = -1;
        sectorDistance[s] = -1.0;
    }
    for (int i = 0; i < stationCount; ++i) {
        const Vec2 delta{map.station(i).position.x - weights.globalCenter.x,
                         map.station(i).position.y - weights.globalCenter.y};
        const double distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (distance < 1e-9) {
            continue;
        }
        double angle = std::atan2(delta.y, delta.x);
        if (angle < 0.0) {
            angle += kTwoPi;
        }
        int sector = static_cast<int>(angle / (kTwoPi / kSectors));
        sector = std::max(0, std::min(kSectors - 1, sector));
        if (distance > sectorDistance[sector]) {
            sectorDistance[sector] = distance;
            sectorStation[sector] = i;
        }
    }
    for (int s = 0; s < kSectors / 2; ++s) {
        addExtremePair(sectorStation[s], sectorStation[s + kSectors / 2]);
    }

    for (std::size_t i = 0; i < anchors.size(); ++i) {
        for (std::size_t j = i + 1; j < anchors.size(); ++j) {
            addCandidate(anchors[i], anchors[j]);
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

    ScoreParams scoreParams = search.score;
    double totalEdgeCost = 0.0;
    for (std::size_t i = 0; i < map.edges().size(); ++i) {
        totalEdgeCost += map.edges()[i].unitCost * map.edges()[i].length;
    }
    double totalWeight = 0.0;
    for (int i = 0; i < stationCount; ++i) {
        totalWeight += weights.weights[static_cast<std::size_t>(i)];
    }
    scoreParams.refCost =
        std::max(1.0, totalEdgeCost + params.kStation * totalWeight);

    double sumOd = 0.0;
    double sumOdDistance = 0.0;
    if (demand.stationCount == stationCount) {
        for (int i = 0; i < stationCount; ++i) {
            for (int j = i + 1; j < stationCount; ++j) {
                const double od =
                    demand.od[static_cast<std::size_t>(i) * stationCount +
                              static_cast<std::size_t>(j)];
                if (od <= 0.0) {
                    continue;
                }
                sumOd += od;
                sumOdDistance += od * euclideanDistance(map.station(i).position,
                                                        map.station(j).position);
            }
        }
    }
    const double averageDistance = sumOd > 0.0 ? sumOdDistance / sumOd : 0.0;
    scoreParams.refTime = std::max(
        1.0, averageDistance * params.ops.segmentRunSec + params.ops.headwaySec * 0.5 +
                 params.ops.dwellSec);

    EvalParams staticParams = params;
    staticParams.evaluateOd = false;
    staticParams.overrideLineOps = true;
    staticParams.connectivity = ConnectivityRequirement::CoveredStations;

    DynamicArray<Line> current;
    DynamicArray<int> currentCandidates;
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
    for (int step = 0; step < search.maxLines; ++step) {
        int best = -1;
        double bestScore = -kInfinity;
        double bestGain = 0.0;
        double bestR1 = 0.0;
        for (std::size_t c = 0; c < candidates.size(); ++c) {
            if (used[c]) {
                continue;
            }
            const Line& candidate = candidates[c];
            if (containsDuplicateLine(current, candidate)) {
                continue;
            }
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
            if (gain <= search.minCoverageGain) {
                continue;
            }
            // 生长阶段也使用同一个代理分数：覆盖、成本、绕行、短线 + 软覆盖下限。
            // R3/R4 在 staticParams 下为 0，不参与生长，交给最终局部搜索。
            const double score = scorePlan(map, trial, metrics, scoreParams);
            if (score > bestScore + 1e-12 ||
                (std::fabs(score - bestScore) <= 1e-12 && gain > bestGain)) {
                best = static_cast<int>(c);
                bestScore = score;
                bestGain = gain;
                bestR1 = metrics.r1;
            }
        }
        if (best < 0) {
            break;
        }
        const Line& chosen = candidates[static_cast<std::size_t>(best)];
        current.push_back(chosen);
        currentCandidates.push_back(best);
        used[static_cast<std::size_t>(best)] = 1;
        for (std::size_t i = 0; i < chosen.stations().size(); ++i) {
            covered[static_cast<std::size_t>(chosen.stations()[i])] = 1;
        }
        currentR1 = bestR1;
        ++result.steps;
    }

    EvalParams finalParams = params;
    finalParams.evaluateOd = true;
    finalParams.overrideLineOps = true;
    finalParams.connectivity = ConnectivityRequirement::CoveredStations;
    result.baseMetrics = evaluate(map, current, weights, demand, finalParams);
    result.metrics = result.baseMetrics;
    result.score = scorePlan(map, current, result.metrics, scoreParams);

    if (search.enableLocalSearch && !current.empty() &&
        search.localSearchBudget > 0) {
        EvalParams localStatic = params;
        localStatic.evaluateOd = false;
        localStatic.overrideLineOps = true;
        localStatic.connectivity = ConnectivityRequirement::CoveredStations;
        const Metrics currentStaticMetrics =
            evaluate(map, current, weights, demand, localStatic);
        double currentStaticScore =
            scorePlan(map, current, currentStaticMetrics, scoreParams);

        double currentScore = result.score;
        int evaluations = 0;
        bool improved = true;

        // 先用便宜静态指标过滤，再对候选做完整 OD 评价。
        auto evaluateTrial = [&](const DynamicArray<Line>& trial, Metrics& metrics,
                                 double& trialStatic) {
            const Metrics staticMetrics =
                evaluate(map, trial, weights, demand, localStatic);
            trialStatic = scorePlan(map, trial, staticMetrics, scoreParams);
            if (search.prefilter &&
                trialStatic + search.prefilterMargin < currentStaticScore) {
                return false;
            }
            ++evaluations;
            ++result.candidatesEvaluated;
            metrics = evaluate(map, trial, weights, demand, finalParams);
            return true;
        };
        while (improved && evaluations < search.localSearchBudget) {
            improved = false;

            // 1) 尝试加入候选线路：必须与已覆盖站点共享至少一个站。
            for (std::size_t c = 0; c < candidates.size(); ++c) {
                if (evaluations >= search.localSearchBudget) {
                    break;
                }
                if (used[c] || static_cast<int>(current.size()) >= search.maxLines) {
                    continue;
                }
                const Line& candidate = candidates[c];
                if (containsDuplicateLine(current, candidate)) {
                    continue;
                }
                bool shares = current.empty();
                for (std::size_t i = 0; i < candidate.stations().size() && !shares; ++i) {
                    shares = covered[static_cast<std::size_t>(candidate.stations()[i])] != 0;
                }
                if (!shares) {
                    continue;
                }
                DynamicArray<Line> trial = current;
                trial.push_back(candidate);
                Metrics metrics;
                double trialStatic = 0.0;
                if (!evaluateTrial(trial, metrics, trialStatic)) {
                    continue;
                }
                const double score = scorePlan(map, trial, metrics, scoreParams);
                if (score > currentScore + 1e-9) {
                    current = static_cast<DynamicArray<Line>&&>(trial);
                    used[c] = 1;
                    currentCandidates.push_back(static_cast<int>(c));
                    for (std::size_t i = 0; i < candidate.stations().size(); ++i) {
                        covered[static_cast<std::size_t>(candidate.stations()[i])] = 1;
                    }
                    currentScore = score;
                    currentStaticScore = trialStatic;
                    result.metrics = metrics;
                    ++result.localMoves;
                    improved = true;
                    break;
                }
            }
            if (improved) {
                continue;
            }

            // 2) 端点延长：沿地图边向线路外延伸一个站。
            for (std::size_t lineIndex = 0; lineIndex < current.size() && !improved; ++lineIndex) {
                const Line& base = current[lineIndex];
                if (base.stationCount() < 2) {
                    continue;
                }
                for (int side = 0; side < 2 && !improved; ++side) {
                    const Index endpoint = side == 0 ? base.stations()[0]
                                                     : base.stations()[base.stations().size() - 1];
                    const DynamicArray<AdjEdge>& neighbors = map.neighbors(endpoint);
                    for (std::size_t n = 0; n < neighbors.size() && !improved; ++n) {
                        if (evaluations >= search.localSearchBudget) {
                            break;
                        }
                        const Index next = neighbors[n].to;
                        if (base.containsStation(next)) {
                            continue;
                        }
                        DynamicArray<Index> stops;
                        if (side == 0) {
                            stops.push_back(next);
                        }
                        for (std::size_t i = 0; i < base.stations().size(); ++i) {
                            stops.push_back(base.stations()[i]);
                        }
                        if (side != 0) {
                            stops.push_back(next);
                        }
                        Line extended = base;
                        extended.setStations(static_cast<DynamicArray<Index>&&>(stops));
                        DynamicArray<Line> trial = current;
                        trial[lineIndex] = static_cast<Line&&>(extended);
                        Metrics metrics;
                double trialStatic = 0.0;
                if (!evaluateTrial(trial, metrics, trialStatic)) {
                    continue;
                }
                        const double score = scorePlan(map, trial, metrics, scoreParams);
                        if (score > currentScore + 1e-9) {
                            used[static_cast<std::size_t>(currentCandidates[lineIndex])] = 0;
                            current = static_cast<DynamicArray<Line>&&>(trial);
                            recomputeCovered(map, current, covered);
                            currentScore = score;
                    currentStaticScore = trialStatic;
                            result.metrics = metrics;
                            ++result.localMoves;
                            improved = true;
                        }
                    }
                }
            }
            if (improved) {
                continue;
            }

            // 3) 端点缩短：去掉线路首/尾一个站。
            for (std::size_t lineIndex = 0; lineIndex < current.size() && !improved; ++lineIndex) {
                const Line& base = current[lineIndex];
                if (base.stationCount() <= std::max(2, search.minLineStations)) {
                    continue;
                }
                for (int side = 0; side < 2 && !improved; ++side) {
                    if (evaluations >= search.localSearchBudget) {
                        break;
                    }
                    DynamicArray<Index> stops;
                    if (side == 0) {
                        for (std::size_t i = 1; i < base.stations().size(); ++i) {
                            stops.push_back(base.stations()[i]);
                        }
                    } else {
                        for (std::size_t i = 0; i + 1 < base.stations().size(); ++i) {
                            stops.push_back(base.stations()[i]);
                        }
                    }
                    Line shortened = base;
                    shortened.setStations(static_cast<DynamicArray<Index>&&>(stops));
                    DynamicArray<Line> trial = current;
                    trial[lineIndex] = static_cast<Line&&>(shortened);
                    Metrics metrics;
                double trialStatic = 0.0;
                if (!evaluateTrial(trial, metrics, trialStatic)) {
                    continue;
                }
                    const double score = scorePlan(map, trial, metrics, scoreParams);
                    if (score > currentScore + 1e-9) {
                        used[static_cast<std::size_t>(currentCandidates[lineIndex])] = 0;
                        current = static_cast<DynamicArray<Line>&&>(trial);
                        recomputeCovered(map, current, covered);
                        currentScore = score;
                    currentStaticScore = trialStatic;
                        result.metrics = metrics;
                        ++result.localMoves;
                        improved = true;
                    }
                }
            }
            if (improved) {
                continue;
            }

            // 4) 中段重构：取线路上两个非相邻站，禁止原线路站点（端点除外）后
            //    求替代路径，替换中间段。
            int rerouteTries = 0;
            DynamicArray<unsigned char> forbidden;
            forbidden.resize(static_cast<std::size_t>(stationCount));
            for (std::size_t lineIndex = 0;
                 lineIndex < current.size() && !improved; ++lineIndex) {
                const Line& base = current[lineIndex];
                if (base.stationCount() < 3) {
                    continue;
                }
                for (std::size_t i = 0; i + 2 < base.stations().size() && !improved; ++i) {
                    for (std::size_t j = i + 2; j < base.stations().size() && !improved;
                         ++j) {
                        if (rerouteTries >= 40 ||
                            evaluations >= search.localSearchBudget) {
                            break;
                        }
                        ++rerouteTries;
                        for (int s = 0; s < stationCount; ++s) {
                            forbidden[static_cast<std::size_t>(s)] = 0;
                        }
                        for (std::size_t k = 0; k < base.stations().size(); ++k) {
                            forbidden[static_cast<std::size_t>(base.stations()[k])] = 1;
                        }
                        const Index a = base.stations()[i];
                        const Index b = base.stations()[j];
                        forbidden[static_cast<std::size_t>(a)] = 0;
                        forbidden[static_cast<std::size_t>(b)] = 0;
                        const DynamicArray<Index> alternative =
                            shortestPathAvoiding(map, a, b, forbidden);
                        if (alternative.size() < 2) {
                            continue;
                        }
                        DynamicArray<Index> stops;
                        for (std::size_t k = 0; k <= i; ++k) {
                            stops.push_back(base.stations()[k]);
                        }
                        for (std::size_t k = 1; k < alternative.size(); ++k) {
                            stops.push_back(alternative[k]);
                        }
                        for (std::size_t k = j + 1; k < base.stations().size(); ++k) {
                            stops.push_back(base.stations()[k]);
                        }
                        Line rerouted = base;
                        rerouted.setStations(static_cast<DynamicArray<Index>&&>(stops));
                        DynamicArray<Line> trial = current;
                        trial[lineIndex] = static_cast<Line&&>(rerouted);
                        Metrics metrics;
                double trialStatic = 0.0;
                if (!evaluateTrial(trial, metrics, trialStatic)) {
                    continue;
                }
                        const double score = scorePlan(map, trial, metrics, scoreParams);
                        if (score > currentScore + 1e-9) {
                            used[static_cast<std::size_t>(currentCandidates[lineIndex])] = 0;
                            current = static_cast<DynamicArray<Line>&&>(trial);
                            recomputeCovered(map, current, covered);
                            currentScore = score;
                    currentStaticScore = trialStatic;
                            result.metrics = metrics;
                            ++result.localMoves;
                            improved = true;
                        }
                    }
                }
            }
            if (improved) {
                continue;
            }

            // 5) 整线替换：保持线路数不变，用候选线替换现有线路。
            for (std::size_t replaceAt = 0;
                 replaceAt < current.size() && !improved; ++replaceAt) {
                for (std::size_t c = 0; c < candidates.size() && !improved; ++c) {
                    if (evaluations >= search.localSearchBudget) {
                        break;
                    }
                    if (used[c]) {
                        continue;
                    }
                    DynamicArray<Line> trial = current;
                    trial[replaceAt] = candidates[c];
                    Metrics metrics;
                double trialStatic = 0.0;
                if (!evaluateTrial(trial, metrics, trialStatic)) {
                    continue;
                }
                    const double score = scorePlan(map, trial, metrics, scoreParams);
                    if (score > currentScore + 1e-9) {
                        used[static_cast<std::size_t>(
                            currentCandidates[replaceAt])] = 0;
                        used[c] = 1;
                        currentCandidates[replaceAt] = static_cast<int>(c);
                        current = static_cast<DynamicArray<Line>&&>(trial);
                        recomputeCovered(map, current, covered);
                        currentScore = score;
                    currentStaticScore = trialStatic;
                        result.metrics = metrics;
                        ++result.localMoves;
                        improved = true;
                    }
                }
            }
            if (improved) {
                continue;
            }

            // 6) 尝试删除线路；删除后仍须满足 CoveredStations 连通。
            for (std::size_t removeAt = 0; removeAt < current.size(); ++removeAt) {
                if (evaluations >= search.localSearchBudget) {
                    break;
                }
                DynamicArray<Line> trial;
                for (std::size_t i = 0; i < current.size(); ++i) {
                    if (i != removeAt) {
                        trial.push_back(current[i]);
                    }
                }
                Metrics metrics;
                double trialStatic = 0.0;
                if (!evaluateTrial(trial, metrics, trialStatic)) {
                    continue;
                }
                const double score = scorePlan(map, trial, metrics, scoreParams);
                if (score > currentScore + 1e-9) {
                    used[static_cast<std::size_t>(
                        currentCandidates[removeAt])] = 0;
                    current = static_cast<DynamicArray<Line>&&>(trial);
                    currentCandidates.erase(removeAt);
                    recomputeCovered(map, current, covered);
                    currentScore = score;
                    currentStaticScore = trialStatic;
                    result.metrics = metrics;
                    ++result.localMoves;
                    improved = true;
                    break;
                }
            }
        }
        result.score = currentScore;
    }

    // 线路级清理：先消除重复线，再尝试同向贯通，最后删除冗余线。
    // 这些操作直接针对“短线碎片/重复线/本应贯通却断开”的结构问题，
    // 与按站增删的局部搜索互补。
    if (search.enableCleanup && !current.empty() && search.cleanupBudget > 0) {
        EvalParams cleanupParams = finalParams;
        cleanupParams.evaluateOd = true;
        double cleanupScore = result.score;
        int cleanupEvaluations = 0;
        bool changed = true;
        int guard = 0;

        while (changed && cleanupEvaluations < search.cleanupBudget && guard < 64) {
            changed = false;
            ++guard;

            // 1) 重复线路：站点集合相同，保留第一条，其余直接删除。
            //    重复线只会增加建设/站台成本，不提供新的覆盖或连通性。
            bool removedDuplicate = false;
            for (std::size_t i = 0; i < current.size() && !removedDuplicate; ++i) {
                for (std::size_t j = i + 1; j < current.size(); ++j) {
                    if (!sameStationSet(current[i], current[j])) {
                        continue;
                    }
                    current.erase(j);
                    ++result.duplicateLinesRemoved;
                    removedDuplicate = true;
                    break;
                }
            }
            if (removedDuplicate) {
                result.metrics = evaluate(map, current, weights, demand, cleanupParams);
                cleanupScore = scorePlan(map, current, result.metrics, scoreParams);
                changed = true;
                continue;
            }

            // 2) 被完全包含的短线：例如某条线只是另一条线的一段。
            //    它不增加新站点或新边，只增加成本与短线惩罚，直接删除。
            bool removedSubset = false;
            for (std::size_t i = 0; i < current.size() && !removedSubset; ++i) {
                for (std::size_t j = 0; j < current.size(); ++j) {
                    if (i == j) {
                        continue;
                    }
                    if (!isStationSubset(current[i], current[j])) {
                        continue;
                    }
                    current.erase(i);
                    ++result.redundantLinesRemoved;
                    removedSubset = true;
                    break;
                }
            }
            if (removedSubset) {
                result.metrics = evaluate(map, current, weights, demand, cleanupParams);
                cleanupScore = scorePlan(map, current, result.metrics, scoreParams);
                changed = true;
                continue;
            }

            // 3) 同向贯通：两条线在端点相接、合并后仍是简单路径，
            //    且转角不超过阈值时，尝试合并为一条线。
            for (std::size_t i = 0; i < current.size() && !changed; ++i) {
                for (std::size_t j = i + 1; j < current.size() && !changed; ++j) {
                    if (cleanupEvaluations >= search.cleanupBudget) {
                        break;
                    }
                    Line merged;
                    double turn = 180.0;
                    if (!mergeLinePair(map, current[i], current[j], merged, &turn)) {
                        continue;
                    }
                    if (turn > search.maxMergeTurnDeg) {
                        continue;
                    }
                    DynamicArray<Line> trial;
                    for (std::size_t k = 0; k < current.size(); ++k) {
                        if (k == i) {
                            trial.push_back(merged);
                        } else if (k != j) {
                            trial.push_back(current[k]);
                        }
                    }
                    ++cleanupEvaluations;
                    ++result.candidatesEvaluated;
                    const Metrics metrics =
                        evaluate(map, trial, weights, demand, cleanupParams);
                    if (!metrics.feasible) {
                        continue;
                    }
                    const double score = scorePlan(map, trial, metrics, scoreParams);
                    if (score > cleanupScore + 1e-9) {
                        current = static_cast<DynamicArray<Line>&&>(trial);
                        cleanupScore = score;
                        result.metrics = metrics;
                        ++result.mergedLines;
                        changed = true;
                    }
                }
            }
            if (changed) {
                continue;
            }

            // 4) 冗余线路：删除后仍可行且代理分数提高，则删除。
            for (std::size_t removeAt = 0; removeAt < current.size() && !changed;
                 ++removeAt) {
                if (cleanupEvaluations >= search.cleanupBudget) {
                    break;
                }
                DynamicArray<Line> trial;
                for (std::size_t k = 0; k < current.size(); ++k) {
                    if (k != removeAt) {
                        trial.push_back(current[k]);
                    }
                }
                ++cleanupEvaluations;
                ++result.candidatesEvaluated;
                const Metrics metrics =
                    evaluate(map, trial, weights, demand, cleanupParams);
                if (!metrics.feasible) {
                    continue;
                }
                const double score = scorePlan(map, trial, metrics, scoreParams);
                if (score > cleanupScore + 1e-9) {
                    current = static_cast<DynamicArray<Line>&&>(trial);
                    cleanupScore = score;
                    result.metrics = metrics;
                    ++result.redundantLinesRemoved;
                    changed = true;
                }
            }
        }
        result.score = cleanupScore;
    }

    result.lines = current;
    return result;
}

}  // namespace plan
}  // namespace uc
