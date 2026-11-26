#include "plan/corridor.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <utility>
#include <vector>

#include "ds/binary_heap.h"
#include "ds/sort.h"
#include "ds/hash_map.h"

namespace uc {
namespace plan {
namespace {

}  // namespace

CorridorGraph buildStationCorridorGraph(const Map& map, const WeightResult& weights,
                                        const DemandResult& demand,
                                        const CorridorOptions& options) {
    CorridorGraph graph;
    const int stationCount = map.stationCount();
    const int edgeCount = static_cast<int>(map.edges().size());
    graph.edgeIsMainCorridor.resize(static_cast<std::size_t>(edgeCount));
    graph.edgeIsRoad.resize(static_cast<std::size_t>(edgeCount));
    for (int i = 0; i < edgeCount; ++i) {
        graph.edgeIsMainCorridor[static_cast<std::size_t>(i)] = 0;
        // 站点/需求驱动的规划不区分背景道路，所有地图边都是候选道路。
        graph.edgeIsRoad[static_cast<std::size_t>(i)] = 1;
    }
    graph.roadEdgeCount = edgeCount;
    graph.connectorEdgeCount = 0;
    if (stationCount < 2 || edgeCount == 0 ||
        static_cast<int>(weights.weights.size()) != stationCount ||
        demand.stationCount != stationCount) {
        return graph;
    }

    // 在站点图上分配 OD 流量：每个起点跑一次 Dijkstra，回溯到各终点累加。
    DynamicArray<double> flow;
    flow.resize(static_cast<std::size_t>(edgeCount));
    for (int i = 0; i < edgeCount; ++i) {
        flow[static_cast<std::size_t>(i)] = 0.0;
    }
    DynamicArray<double> dist;
    DynamicArray<int> previousEdge;
    DynamicArray<int> previousStation;
    dist.resize(static_cast<std::size_t>(stationCount));
    previousEdge.resize(static_cast<std::size_t>(stationCount));
    previousStation.resize(static_cast<std::size_t>(stationCount));

    for (int origin = 0; origin < stationCount; ++origin) {
        for (int i = 0; i < stationCount; ++i) {
            dist[static_cast<std::size_t>(i)] = kInfinity;
            previousEdge[static_cast<std::size_t>(i)] = -1;
            previousStation[static_cast<std::size_t>(i)] = -1;
        }
        BinaryMinHeap heap;
        dist[static_cast<std::size_t>(origin)] = 0.0;
        heap.push(0.0, origin);
        while (!heap.empty()) {
            const HeapItem item = heap.pop();
            const Index station = item.value;
            if (item.key > dist[static_cast<std::size_t>(station)] + 1e-9) {
                continue;
            }
            const DynamicArray<AdjEdge>& neighbors = map.neighbors(station);
            for (std::size_t n = 0; n < neighbors.size(); ++n) {
                const AdjEdge& edge = neighbors[n];
                const double next = item.key + edge.length;
                if (next + 1e-9 < dist[static_cast<std::size_t>(edge.to)]) {
                    dist[static_cast<std::size_t>(edge.to)] = next;
                    previousEdge[static_cast<std::size_t>(edge.to)] = edge.edgeId;
                    previousStation[static_cast<std::size_t>(edge.to)] = station;
                    heap.push(next, edge.to);
                }
            }
        }
        for (int destination = origin + 1; destination < stationCount; ++destination) {
            const double od =
                demand.od[static_cast<std::size_t>(origin) * stationCount +
                          static_cast<std::size_t>(destination)];
            if (od <= 0.0 || std::isinf(dist[static_cast<std::size_t>(destination)])) {
                continue;
            }
            int current = destination;
            int guard = 0;
            while (current != origin && guard++ <= stationCount) {
                const int edge = previousEdge[static_cast<std::size_t>(current)];
                if (edge < 0) {
                    break;
                }
                flow[static_cast<std::size_t>(edge)] += od;
                current = previousStation[static_cast<std::size_t>(current)];
            }
        }
    }

    // 取流量最高的一段边作为主走廊；其余边仍然是普通道路。
    DynamicArray<int> order;
    for (int i = 0; i < edgeCount; ++i) {
        if (flow[static_cast<std::size_t>(i)] > 1e-9) {
            order.push_back(i);
        }
    }
    quickSort(order, [&flow](int a, int b) {
        return flow[static_cast<std::size_t>(a)] > flow[static_cast<std::size_t>(b)];
    });
    const double ratio = std::max(0.0, std::min(1.0, options.corridorEdgeRatio));
    const int budget = std::max(
        2, std::min(static_cast<int>(order.size()),
                    static_cast<int>(std::ceil(order.size() * ratio))));
    for (int i = 0; i < budget; ++i) {
        const int edge = order[static_cast<std::size_t>(i)];
        graph.edgeIsMainCorridor[static_cast<std::size_t>(edge)] = 1;
        ++graph.mainCorridorEdgeCount;
        graph.totalRoadLength += map.edges()[static_cast<std::size_t>(edge)].length;
    }

    // 把高流量边收缩成走廊段（度 2 链）。
    DynamicArray<DynamicArray<std::pair<Index, int>>> adjacency;
    adjacency.resize(static_cast<std::size_t>(stationCount));
    DynamicArray<int> degree;
    degree.resize(static_cast<std::size_t>(stationCount));
    for (int i = 0; i < stationCount; ++i) {
        degree[static_cast<std::size_t>(i)] = 0;
    }
    for (int i = 0; i < edgeCount; ++i) {
        if (!graph.edgeIsMainCorridor[static_cast<std::size_t>(i)]) {
            continue;
        }
        const EdgeRecord& edge = map.edges()[static_cast<std::size_t>(i)];
        adjacency[static_cast<std::size_t>(edge.from)].push_back(
            std::make_pair(edge.to, i));
        adjacency[static_cast<std::size_t>(edge.to)].push_back(
            std::make_pair(edge.from, i));
        ++degree[static_cast<std::size_t>(edge.from)];
        ++degree[static_cast<std::size_t>(edge.to)];
    }
    for (int i = 0; i < stationCount; ++i) {
        if (degree[static_cast<std::size_t>(i)] != 2 && degree[static_cast<std::size_t>(i)] > 0) {
            ++graph.junctionCount;
        }
    }

    DynamicArray<unsigned char> visited;
    visited.resize(static_cast<std::size_t>(edgeCount));
    for (int i = 0; i < edgeCount; ++i) {
        visited[static_cast<std::size_t>(i)] = 0;
    }
    auto buildSegment = [&](Index start, int firstEdge) {
        CorridorSegment segment;
        segment.id = static_cast<int>(graph.segments.size());
        segment.stations.push_back(start);
        int edgeIndex = firstEdge;
        Index current = map.edges()[static_cast<std::size_t>(edgeIndex)].from == start
                            ? map.edges()[static_cast<std::size_t>(edgeIndex)].to
                            : map.edges()[static_cast<std::size_t>(edgeIndex)].from;
        while (edgeIndex >= 0) {
            visited[static_cast<std::size_t>(edgeIndex)] = 1;
            ++segment.mapEdgeCount;
            segment.length += map.edges()[static_cast<std::size_t>(edgeIndex)].length;
            segment.stations.push_back(current);
            if (degree[static_cast<std::size_t>(current)] != 2) {
                break;
            }
            int nextEdge = -1;
            Index nextStation = kInvalidIndex;
            const DynamicArray<std::pair<Index, int>>& neighbors =
                adjacency[static_cast<std::size_t>(current)];
            for (std::size_t n = 0; n < neighbors.size(); ++n) {
                if (neighbors[n].second == edgeIndex ||
                    visited[static_cast<std::size_t>(neighbors[n].second)]) {
                    continue;
                }
                nextEdge = neighbors[n].second;
                nextStation = neighbors[n].first;
                break;
            }
            if (nextEdge < 0) {
                break;
            }
            current = nextStation;
            edgeIndex = nextEdge;
        }
        if (segment.stations.size() >= 2) {
            graph.maxSegmentStations =
                std::max(graph.maxSegmentStations, static_cast<int>(segment.stations.size()));
            graph.segments.push_back(static_cast<CorridorSegment&&>(segment));
        }
    };

    for (int station = 0; station < stationCount; ++station) {
        if (degree[static_cast<std::size_t>(station)] == 2 ||
            degree[static_cast<std::size_t>(station)] == 0) {
            continue;
        }
        const DynamicArray<std::pair<Index, int>>& neighbors =
            adjacency[static_cast<std::size_t>(station)];
        for (std::size_t n = 0; n < neighbors.size(); ++n) {
            if (!visited[static_cast<std::size_t>(neighbors[n].second)]) {
                buildSegment(static_cast<Index>(station), neighbors[n].second);
            }
        }
    }
    for (int i = 0; i < edgeCount; ++i) {
        if (graph.edgeIsMainCorridor[static_cast<std::size_t>(i)] &&
            !visited[static_cast<std::size_t>(i)]) {
            buildSegment(map.edges()[static_cast<std::size_t>(i)].from, i);
        }
    }

    return graph;
}

CorridorLineUsage corridorLineUsage(const Map& map, const CorridorGraph& graph,
                                   const DynamicArray<Line>& lines) {
    CorridorLineUsage usage;
    for (std::size_t l = 0; l < lines.size(); ++l) {
        const Line& line = lines[l];
        for (int i = 0; i + 1 < line.stationCount(); ++i) {
            const Index a = line.stations()[static_cast<std::size_t>(i)];
            const Index b = line.stations()[static_cast<std::size_t>(i + 1)];
            int edgeIndex = -1;
            for (std::size_t e = 0; e < map.edges().size(); ++e) {
                const EdgeRecord& edge = map.edges()[e];
                if ((edge.from == a && edge.to == b) || (edge.from == b && edge.to == a)) {
                    edgeIndex = static_cast<int>(e);
                    break;
                }
            }
            if (edgeIndex < 0) {
                continue;
            }
            ++usage.lineEdges;
            if (edgeIndex < static_cast<int>(graph.edgeIsMainCorridor.size()) &&
                graph.edgeIsMainCorridor[static_cast<std::size_t>(edgeIndex)]) {
                ++usage.mainCorridorEdges;
            }
            if (edgeIndex < static_cast<int>(graph.edgeIsRoad.size()) &&
                graph.edgeIsRoad[static_cast<std::size_t>(edgeIndex)]) {
                ++usage.roadEdges;
            } else {
                ++usage.connectorEdges;
            }
        }
    }
    return usage;
}

void printCorridorLineUsage(const CorridorLineUsage& usage) {
    const double mainRatio = usage.lineEdges > 0
                                 ? 100.0 * usage.mainCorridorEdges / usage.lineEdges
                                 : 0.0;
    std::cout << "line corridor usage: edges=" << usage.lineEdges
              << " mainCorridor=" << usage.mainCorridorEdges
              << " (" << mainRatio << "%)"
              << " other=" << (usage.lineEdges - usage.mainCorridorEdges) << '\n';
}

void printCorridorGraph(const CorridorGraph& graph) {
    std::cout << "corridor graph: mainCorridorEdges=" << graph.mainCorridorEdgeCount
              << " roadEdges=" << graph.roadEdgeCount
              << " connectorEdges=" << graph.connectorEdgeCount
              << " junctionStations=" << graph.junctionCount
              << " segments=" << graph.segments.size()
              << " maxSegmentStations=" << graph.maxSegmentStations << '\n';
    std::cout << "road length=" << graph.totalRoadLength << '\n';
    for (std::size_t i = 0; i < graph.segments.size(); ++i) {
        const CorridorSegment& segment = graph.segments[i];
        std::cout << "  C" << segment.id << ": stations=" << segment.stations.size()
                  << " length=" << segment.length << " roadWidth=" << segment.roadWidth
                  << '\n';
    }
}


double turnDeviationDegrees(const Vec2& a, const Vec2& b, const Vec2& c) {
    const double ax = b.x - a.x;
    const double ay = b.y - a.y;
    const double bx = c.x - b.x;
    const double by = c.y - b.y;
    const double la = std::sqrt(ax * ax + ay * ay);
    const double lb = std::sqrt(bx * bx + by * by);
    if (la < 1e-12 || lb < 1e-12) {
        return 180.0;
    }
    double cosine = (ax * bx + ay * by) / (la * lb);
    cosine = std::max(-1.0, std::min(1.0, cosine));
    return std::acos(cosine) * 180.0 / 3.14159265358979323846;
}

DynamicArray<Index> reversePath(const DynamicArray<Index>& path) {
    DynamicArray<Index> reversed;
    for (std::size_t i = path.size(); i > 0; --i) {
        reversed.push_back(path[i - 1]);
    }
    return reversed;
}

bool pathContains(const DynamicArray<Index>& path, Index station) {
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] == station) {
            return true;
        }
    }
    return false;
}

DynamicArray<Index> pathSide(const DynamicArray<Index>& path, std::size_t junction, bool left) {
    DynamicArray<Index> side;
    if (left) {
        for (std::size_t i = 0; i <= junction; ++i) {
            side.push_back(path[i]);
        }
    } else {
        for (std::size_t i = junction; i < path.size(); ++i) {
            side.push_back(path[i]);
        }
    }
    return side;
}

DynamicArray<DynamicArray<Index>> buildCorridorLineCandidates(
    const Map& map, const CorridorGraph& graph, double maxTurnDeviationDeg, int limit) {
    DynamicArray<DynamicArray<Index>> result;
    auto addPath = [&](const DynamicArray<Index>& path) {
        if (static_cast<int>(result.size()) >= limit || path.size() < 2) {
            return;
        }
        // 简单路径：不允许重复站点。
        for (std::size_t i = 0; i < path.size(); ++i) {
            for (std::size_t j = i + 1; j < path.size(); ++j) {
                if (path[i] == path[j]) {
                    return;
                }
            }
        }
        for (std::size_t i = 0; i < result.size(); ++i) {
            if (result[i].size() != path.size()) {
                continue;
            }
            bool same = true;
            for (std::size_t j = 0; j < path.size(); ++j) {
                if (result[i][j] != path[j]) {
                    same = false;
                    break;
                }
            }
            if (same) {
                return;
            }
        }
        result.push_back(path);
    };

    for (std::size_t i = 0; i < graph.segments.size(); ++i) {
        addPath(graph.segments[i].stations);
    }

    for (std::size_t i = 0; i < graph.segments.size(); ++i) {
        const DynamicArray<Index>& a = graph.segments[i].stations;
        for (std::size_t j = i + 1; j < graph.segments.size(); ++j) {
            const DynamicArray<Index>& b = graph.segments[j].stations;
            for (std::size_t ia = 0; ia < a.size(); ++ia) {
                for (std::size_t ib = 0; ib < b.size(); ++ib) {
                    if (a[ia] != b[ib]) {
                        continue;
                    }
                    const Index junction = a[ia];
                    for (int sideA = 0; sideA < 2; ++sideA) {
                        for (int sideB = 0; sideB < 2; ++sideB) {
                            DynamicArray<Index> left = pathSide(a, ia, sideA == 0);
                            DynamicArray<Index> right = pathSide(b, ib, sideB == 0);
                            if (left.empty() || right.empty()) {
                                continue;
                            }
                            if (left.back() != junction) {
                                left = reversePath(left);
                            }
                            if (right.front() != junction) {
                                right = reversePath(right);
                            }
                            if (left.back() != junction || right.front() != junction) {
                                continue;
                            }
                            if (left.size() >= 2 && right.size() >= 2) {
                                const Vec2& p = map.station(left[left.size() - 2]).position;
                                const Vec2& q = map.station(junction).position;
                                const Vec2& r = map.station(right[1]).position;
                                if (turnDeviationDegrees(p, q, r) > maxTurnDeviationDeg) {
                                    continue;
                                }
                            }
                            DynamicArray<Index> merged = left;
                            for (std::size_t k = 1; k < right.size(); ++k) {
                                merged.push_back(right[k]);
                            }
                            addPath(merged);
                        }
                    }
                }
            }
        }
    }
    // 走廊链式延伸：在候选线路两端再接一段走廊，允许一条线串起 3 段以上主干。
    const std::size_t chainBase = result.size();
    for (std::size_t index = 0; index < chainBase; ++index) {
        const DynamicArray<Index> base = result[index];
        if (base.size() < 2) {
            continue;
        }
        for (int side = 0; side < 2; ++side) {
            const Index endpoint = side == 0 ? base.front() : base.back();
            for (std::size_t seg = 0; seg < graph.segments.size(); ++seg) {
                const DynamicArray<Index>& stations = graph.segments[seg].stations;
                for (std::size_t pos = 0; pos < stations.size(); ++pos) {
                    if (stations[pos] != endpoint) {
                        continue;
                    }
                    for (int direction = 0; direction < 2; ++direction) {
                        DynamicArray<Index> sidePath = pathSide(stations, pos, direction == 0);
                        if (sidePath.size() < 2) {
                            continue;
                        }
                        if (sidePath.front() != endpoint) {
                            sidePath = reversePath(sidePath);
                        }
                        if (sidePath.front() != endpoint) {
                            continue;
                        }
                        if (side == 0) {
                            const Vec2& p = map.station(sidePath[1]).position;
                            const Vec2& q = map.station(endpoint).position;
                            const Vec2& r = map.station(base[1]).position;
                            if (turnDeviationDegrees(p, q, r) > maxTurnDeviationDeg) {
                                continue;
                            }
                            DynamicArray<Index> merged;
                            for (std::size_t k = 0; k + 1 < sidePath.size(); ++k) {
                                merged.push_back(sidePath[k]);
                            }
                            for (std::size_t k = 0; k < base.size(); ++k) {
                                merged.push_back(base[k]);
                            }
                            addPath(merged);
                        } else {
                            const Vec2& p = map.station(base[base.size() - 2]).position;
                            const Vec2& q = map.station(endpoint).position;
                            const Vec2& r = map.station(sidePath[1]).position;
                            if (turnDeviationDegrees(p, q, r) > maxTurnDeviationDeg) {
                                continue;
                            }
                            DynamicArray<Index> merged = base;
                            for (std::size_t k = 1; k < sidePath.size(); ++k) {
                                merged.push_back(sidePath[k]);
                            }
                            addPath(merged);
                        }
                    }
                }
            }
        }
    }

    // 端点连接：只允许在线路两端各接入最多 2 个连接边相邻站，
    // 用于服务不在走廊上的外围站点，避免中段穿越街区。
    const std::size_t baseCount = result.size();
    for (std::size_t index = 0; index < baseCount; ++index) {
        const DynamicArray<Index> base = result[index];
        if (base.size() < 2) {
            continue;
        }
        for (int side = 0; side < 2; ++side) {
            const Index endpoint = side == 0 ? base.front() : base.back();
            std::vector<std::pair<double, Index>> access;
            const DynamicArray<AdjEdge>& neighbors = map.neighbors(endpoint);
            for (std::size_t n = 0; n < neighbors.size(); ++n) {
                const AdjEdge& edge = neighbors[n];
                if (edge.edgeId < 0 ||
                    edge.edgeId >= static_cast<int>(graph.edgeIsMainCorridor.size()) ||
                    graph.edgeIsMainCorridor[static_cast<std::size_t>(edge.edgeId)]) {
                    continue;
                }
                if (pathContains(base, edge.to)) {
                    continue;
                }
                access.push_back(std::make_pair(
                    euclideanDistance(map.station(endpoint).position,
                                      map.station(edge.to).position),
                    edge.to));
            }
            std::sort(access.begin(), access.end());
            for (std::size_t n = 0; n < access.size() && n < 2; ++n) {
                DynamicArray<Index> variant;
                if (side == 0) {
                    variant.push_back(access[n].second);
                    for (std::size_t k = 0; k < base.size(); ++k) {
                        variant.push_back(base[k]);
                    }
                } else {
                    for (std::size_t k = 0; k < base.size(); ++k) {
                        variant.push_back(base[k]);
                    }
                    variant.push_back(access[n].second);
                }
                addPath(variant);
            }
        }
    }

    return result;
}

}  // namespace plan
}  // namespace uc
