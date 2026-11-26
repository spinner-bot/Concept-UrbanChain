#include "plan/corridor.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <utility>
#include <vector>

#include "core/background.h"
#include "ds/hash_map.h"

namespace uc {
namespace plan {
namespace {

struct Projection {
    bool found = false;
    double distance = kInfinity;
    double arc = 0.0;
};

double pointSegmentDistance(const Vec2& point, const Vec2& a, const Vec2& b, double* tOut) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared < 1e-12) {
        if (tOut != nullptr) {
            *tOut = 0.0;
        }
        return euclideanDistance(point, a);
    }
    double t = ((point.x - a.x) * dx + (point.y - a.y) * dy) / lengthSquared;
    t = std::max(0.0, std::min(1.0, t));
    if (tOut != nullptr) {
        *tOut = t;
    }
    return euclideanDistance(point, Vec2{a.x + t * dx, a.y + t * dy});
}

Projection projectOntoPolyline(const Vec2& point, const DynamicArray<Vec2>& points) {
    Projection best;
    double arc = 0.0;
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        double t = 0.0;
        const double distance = pointSegmentDistance(point, points[i], points[i + 1], &t);
        const double segmentLength = euclideanDistance(points[i], points[i + 1]);
        if (distance < best.distance) {
            best.found = true;
            best.distance = distance;
            best.arc = arc + t * segmentLength;
        }
        arc += segmentLength;
    }
    return best;
}

std::uint64_t edgeKey(Index a, Index b) {
    const std::uint32_t low = static_cast<std::uint32_t>(std::min(a, b));
    const std::uint32_t high = static_cast<std::uint32_t>(std::max(a, b));
    return (static_cast<std::uint64_t>(low) << 32) | high;
}

}  // namespace

CorridorGraph buildCorridorGraph(const Map& map, const CorridorOptions& options) {
    CorridorGraph graph;
    const int edgeCount = static_cast<int>(map.edges().size());
    const int stationCount = map.stationCount();
    graph.edgeIsRoad.resize(static_cast<std::size_t>(edgeCount));
    for (int i = 0; i < edgeCount; ++i) {
        graph.edgeIsRoad[static_cast<std::size_t>(i)] = 0;
    }

    HashMap<std::uint64_t, int, IntHash> edgeIndex;
    for (int i = 0; i < edgeCount; ++i) {
        const EdgeRecord& edge = map.edges()[static_cast<std::size_t>(i)];
        edgeIndex.insert(edgeKey(edge.from, edge.to), i);
    }
    DynamicArray<unsigned char> usedEdge;
    usedEdge.resize(static_cast<std::size_t>(edgeCount));
    for (int i = 0; i < edgeCount; ++i) {
        usedEdge[static_cast<std::size_t>(i)] = 0;
    }

    // 每条背景道路折线是一条候选走廊；只吸收主/次干道，局部街巷不进入走廊。
    for (std::size_t f = 0; f < map.backgroundFeatures().size(); ++f) {
        const BackgroundFeature& feature = map.backgroundFeatures()[f];
        if (feature.kind != BackgroundKind::Road || feature.points.size() < 2) {
            continue;
        }
        if (feature.width + 1e-9 < options.minCorridorWidth) {
            continue;
        }

        std::vector<std::pair<double, Index>> stationsOnRoad;
        for (int station = 0; station < stationCount; ++station) {
            const Projection projection =
                projectOntoPolyline(map.station(station).position, feature.points);
            if (projection.found && projection.distance <= options.roadTolerance) {
                stationsOnRoad.push_back(
                    std::make_pair(projection.arc, static_cast<Index>(station)));
            }
        }
        if (stationsOnRoad.size() < 2) {
            continue;
        }
        std::sort(stationsOnRoad.begin(), stationsOnRoad.end());

        auto flush = [&](std::size_t begin, std::size_t endExclusive) {
            if (endExclusive - begin < 2) {
                return;
            }
            CorridorSegment segment;
            segment.id = static_cast<int>(graph.segments.size());
            segment.roadWidth = feature.width;
            double length = 0.0;
            int edgeUsedCount = 0;
            for (std::size_t i = begin; i < endExclusive; ++i) {
                segment.stations.push_back(stationsOnRoad[i].second);
            }
            for (std::size_t i = begin; i + 1 < endExclusive; ++i) {
                const Index a = stationsOnRoad[i].second;
                const Index b = stationsOnRoad[i + 1].second;
                const int* found = edgeIndex.find(edgeKey(a, b));
                if (found == nullptr) {
                    return;  // 理论上已按连续边切分，这里保护
                }
                const int edge = *found;
                if (usedEdge[static_cast<std::size_t>(edge)]) {
                    return;
                }
                usedEdge[static_cast<std::size_t>(edge)] = 1;
                graph.edgeIsRoad[static_cast<std::size_t>(edge)] = 1;
                length += map.edges()[static_cast<std::size_t>(edge)].length;
                ++edgeUsedCount;
            }
            if (edgeUsedCount == 0) {
                return;
            }
            segment.length = length;
            segment.mapEdgeCount = edgeUsedCount;
            graph.maxSegmentStations =
                std::max(graph.maxSegmentStations, static_cast<int>(segment.stations.size()));
            graph.segments.push_back(static_cast<CorridorSegment&&>(segment));
        };

        std::size_t runBegin = 0;
        for (std::size_t i = 1; i < stationsOnRoad.size(); ++i) {
            const Index a = stationsOnRoad[i - 1].second;
            const Index b = stationsOnRoad[i].second;
            const int* found = edgeIndex.find(edgeKey(a, b));
            const bool continuous =
                found != nullptr && !usedEdge[static_cast<std::size_t>(*found)];
            if (!continuous) {
                flush(runBegin, i);
                runBegin = i;
            }
        }
        flush(runBegin, stationsOnRoad.size());
    }

    // 统计与走廊无关的边：kNN 连接边、局部街巷等。
    for (int i = 0; i < edgeCount; ++i) {
        if (graph.edgeIsRoad[static_cast<std::size_t>(i)]) {
            ++graph.roadEdgeCount;
            graph.totalRoadLength += map.edges()[static_cast<std::size_t>(i)].length;
        } else {
            ++graph.connectorEdgeCount;
        }
    }

    // 走廊共享站点数：出现在两条及以上走廊中的站点，视为走廊交点/换乘节点。
    HashMap<int, int, IntHash> stationIncidence;
    for (std::size_t i = 0; i < graph.segments.size(); ++i) {
        const CorridorSegment& segment = graph.segments[i];
        for (std::size_t s = 0; s < segment.stations.size(); ++s) {
            const Index station = segment.stations[s];
            const int* count = stationIncidence.find(station);
            stationIncidence.set(station, count == nullptr ? 1 : *count + 1);
        }
    }
    stationIncidence.forEach([&graph](int, int count) {
        if (count >= 2) {
            ++graph.junctionCount;
        }
    });

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
    const double ratio = usage.lineEdges > 0
                             ? 100.0 * usage.roadEdges / usage.lineEdges
                             : 0.0;
    std::cout << "line corridor usage: edges=" << usage.lineEdges
              << " road=" << usage.roadEdges
              << " connector=" << usage.connectorEdges
              << " roadRatio=" << ratio << "%\n";
}

void printCorridorGraph(const CorridorGraph& graph) {
    std::cout << "corridor graph: roadEdges=" << graph.roadEdgeCount
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
                    edge.edgeId >= static_cast<int>(graph.edgeIsRoad.size()) ||
                    graph.edgeIsRoad[static_cast<std::size_t>(edge.edgeId)]) {
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
