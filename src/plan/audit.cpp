#include "plan/audit.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace uc {
namespace plan {
namespace {

constexpr double kPi = 3.14159265358979323846;

LineAxis classifyVector(const Vec2& vector) {
    const double length = std::sqrt(vector.x * vector.x + vector.y * vector.y);
    if (length < 1e-9) {
        return LineAxis::Diagonal;
    }
    double degrees = std::fabs(std::atan2(vector.y, vector.x) * 180.0 / kPi);
    if (degrees > 90.0) {
        degrees = 180.0 - degrees;
    }
    if (degrees <= 22.5) {
        return LineAxis::EastWest;
    }
    if (degrees < 67.5) {
        return LineAxis::Diagonal;
    }
    return LineAxis::NorthSouth;
}

void reverseInPlace(DynamicArray<Index>& values) {
    for (std::size_t i = 0, j = values.size(); i + 1 < j; ++i, --j) {
        const Index tmp = values[i];
        values[i] = values[j - 1];
        values[j - 1] = tmp;
    }
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

double turnDegrees(const Map& map, const DynamicArray<Index>& first,
                   const DynamicArray<Index>& second) {
    if (first.size() < 2 || second.size() < 2) {
        return 180.0;
    }
    const Vec2 before = map.station(first[first.size() - 2]).position;
    const Vec2 junction = map.station(first[first.size() - 1]).position;
    const Vec2 after = map.station(second[1]).position;
    const Vec2 incoming{junction.x - before.x, junction.y - before.y};
    const Vec2 outgoing{after.x - junction.x, after.y - junction.y};
    const double incomingLength = std::sqrt(incoming.x * incoming.x +
                                            incoming.y * incoming.y);
    const double outgoingLength = std::sqrt(outgoing.x * outgoing.x +
                                            outgoing.y * outgoing.y);
    if (incomingLength < 1e-9 || outgoingLength < 1e-9) {
        return 180.0;
    }
    double cosine = (incoming.x * outgoing.x + incoming.y * outgoing.y) /
                    (incomingLength * outgoingLength);
    cosine = std::max(-1.0, std::min(1.0, cosine));
    return std::acos(cosine) * 180.0 / kPi;
}

bool buildMergedPath(const Map& map, const Line& first, bool firstReverse,
                     const Line& second, bool secondReverse,
                     DynamicArray<Index>& merged, double& turn) {
    DynamicArray<Index> a = first.stations();
    DynamicArray<Index> b = second.stations();
    if (firstReverse) {
        reverseInPlace(a);
    }
    if (secondReverse) {
        reverseInPlace(b);
    }
    if (a.empty() || b.empty() || a.back() != b.front()) {
        return false;
    }

    turn = turnDegrees(map, a, b);
    DynamicArray<unsigned char> used;
    used.resize(static_cast<std::size_t>(map.stationCount()));
    for (int i = 0; i < map.stationCount(); ++i) {
        used[static_cast<std::size_t>(i)] = 0;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] < 0 || a[i] >= map.stationCount()) {
            return false;
        }
        used[static_cast<std::size_t>(a[i])] = 1;
    }
    for (std::size_t i = 1; i < b.size(); ++i) {
        if (b[i] < 0 || b[i] >= map.stationCount()) {
            return false;
        }
        if (used[static_cast<std::size_t>(b[i])]) {
            return false;
        }
        used[static_cast<std::size_t>(b[i])] = 1;
    }

    merged.clear();
    for (std::size_t i = 0; i < a.size(); ++i) {
        merged.push_back(a[i]);
    }
    for (std::size_t i = 1; i < b.size(); ++i) {
        merged.push_back(b[i]);
    }
    if (merged.size() < 2) {
        return false;
    }
    for (std::size_t i = 0; i + 1 < merged.size(); ++i) {
        if (!map.hasEdge(merged[i], merged[i + 1])) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool mergeLinePair(const Map& map, const Line& first, const Line& second,
                   Line& merged, double* turnDeg) {
    if (first.stationCount() < 2 || second.stationCount() < 2) {
        return false;
    }
    bool found = false;
    double bestTurn = 180.0;
    DynamicArray<Index> bestStops;
    for (int firstSide = 0; firstSide < 2; ++firstSide) {
        for (int secondSide = 0; secondSide < 2; ++secondSide) {
            const Index firstEndpoint = firstSide == 0
                                            ? first.stations().front()
                                            : first.stations().back();
            const Index secondEndpoint = secondSide == 0
                                             ? second.stations().front()
                                             : second.stations().back();
            if (firstEndpoint != secondEndpoint) {
                continue;
            }
            DynamicArray<Index> candidate;
            double turn = 180.0;
            if (!buildMergedPath(map, first, firstSide == 0, second,
                                 secondSide == 1, candidate, turn)) {
                continue;
            }
            if (!found || turn < bestTurn) {
                found = true;
                bestTurn = turn;
                bestStops = static_cast<DynamicArray<Index>&&>(candidate);
            }
        }
    }
    if (!found) {
        return false;
    }

    merged = Line(first.id(), first.name() + "+" + second.name(), first.color());
    merged.setStations(static_cast<DynamicArray<Index>&&>(bestStops));
    merged.ops().dwellSec = first.ops().dwellSec;
    merged.ops().headway = first.ops().headway;
    if (merged.ops().headway.empty()) {
        merged.ops().headway.push_back(HeadwayPeriod{0, 86400, 300});
    }
    merged.ops().segmentRunSec.clear();
    for (int i = 0; i + 1 < merged.stationCount(); ++i) {
        double run = 120.0;
        if (static_cast<std::size_t>(i) < first.ops().segmentRunSec.size()) {
            run = first.ops().segmentRunSec[static_cast<std::size_t>(i)];
        }
        merged.ops().segmentRunSec.push_back(run);
    }
    if (turnDeg != nullptr) {
        *turnDeg = bestTurn;
    }
    return true;
}

namespace {

double axisEntropy(const int counts[3]) {
    const int total = counts[0] + counts[1] + counts[2];
    int usedCategories = 0;
    for (int i = 0; i < 3; ++i) {
        if (counts[i] > 0) {
            ++usedCategories;
        }
    }
    if (total <= 0 || usedCategories <= 1) {
        return 0.0;
    }
    double entropy = 0.0;
    for (int i = 0; i < 3; ++i) {
        if (counts[i] <= 0) {
            continue;
        }
        const double p = static_cast<double>(counts[i]) / static_cast<double>(total);
        entropy -= p * std::log(p);
    }
    return entropy / std::log(static_cast<double>(usedCategories));
}

std::uint64_t edgeKey(Index a, Index b) {
    const std::uint32_t low = static_cast<std::uint32_t>(std::min(a, b));
    const std::uint32_t high = static_cast<std::uint32_t>(std::max(a, b));
    return (static_cast<std::uint64_t>(low) << 32) | high;
}

}  // namespace

MapGeometryReport auditMapGeometry(const Map& map, double tolerance) {
    MapGeometryReport report;
    report.edgeCount = static_cast<int>(map.edges().size());
    for (std::size_t i = 0; i < map.edges().size(); ++i) {
        const EdgeRecord& edge = map.edges()[i];
        const double geometric = euclideanDistance(
            map.station(edge.from).position, map.station(edge.to).position);
        if (geometric < 1e-9 || edge.length <= 0.0) {
            continue;
        }
        const double ratio = edge.length / geometric;
        report.worstRatio = std::max(report.worstRatio, ratio);
        const double allowance = tolerance * std::max(1.0, geometric) + 0.2;
        if (std::fabs(edge.length - geometric) <= allowance) {
            continue;
        }
        ++report.mismatchedEdges;
        MapGeometryIssue issue;
        issue.from = edge.from;
        issue.to = edge.to;
        issue.geometricLength = geometric;
        issue.storedLength = edge.length;
        issue.ratio = ratio;
        report.issues.push_back(issue);
    }
    return report;
}

void printMapGeometry(const Map& map, const MapGeometryReport& report) {
    std::cout << "map geometry: edges=" << report.edgeCount
              << " mismatched=" << report.mismatchedEdges
              << " worstLengthRatio=" << report.worstRatio << '\n';
    for (std::size_t i = 0; i < report.issues.size(); ++i) {
        const MapGeometryIssue& issue = report.issues[i];
        std::cout << "  " << map.station(issue.from).id << " - "
                  << map.station(issue.to).id
                  << " stored=" << issue.storedLength
                  << " geometric=" << issue.geometricLength
                  << " ratio=" << issue.ratio << '\n';
    }
}

LineStructure auditLines(const Map& map, const DynamicArray<Line>& lines,
                         int shortStationThreshold) {
    LineStructure structure;
    structure.stationCount = map.stationCount();
    structure.lineCount = static_cast<int>(lines.size());
    structure.shortStationThreshold = shortStationThreshold;

    DynamicArray<int> lineCountPerStation;
    lineCountPerStation.resize(static_cast<std::size_t>(map.stationCount()));
    for (int i = 0; i < map.stationCount(); ++i) {
        lineCountPerStation[static_cast<std::size_t>(i)] = 0;
    }

    DynamicArray<std::uint64_t> uniqueEdgeKeys;
    DynamicArray<int> uniqueEdgeAxes;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const Line& line = lines[i];
        if (line.stationCount() < 2) {
            continue;
        }
        if (line.stationCount() <= shortStationThreshold) {
            ++structure.shortLines;
        }
        const Vec2 first = map.station(line.stations()[0]).position;
        const Vec2 last = map.station(line.stations()[line.stations().size() - 1]).position;
        const LineAxis axis = classifyVector(Vec2{last.x - first.x, last.y - first.y});
        ++structure.axisLines[static_cast<int>(axis)];

        for (int j = 0; j < line.stationCount(); ++j) {
            const Index station = line.stations()[static_cast<std::size_t>(j)];
            if (station >= 0 && station < map.stationCount()) {
                ++lineCountPerStation[static_cast<std::size_t>(station)];
            }
        }
        for (int j = 0; j + 1 < line.stationCount(); ++j) {
            const Index a = line.stations()[static_cast<std::size_t>(j)];
            const Index b = line.stations()[static_cast<std::size_t>(j + 1)];
            if (!map.hasEdge(a, b)) {
                continue;
            }
            const std::uint64_t key = edgeKey(a, b);
            bool seen = false;
            for (std::size_t k = 0; k < uniqueEdgeKeys.size(); ++k) {
                if (uniqueEdgeKeys[k] == key) {
                    seen = true;
                    break;
                }
            }
            if (seen) {
                continue;
            }
            const Vec2 pa = map.station(a).position;
            const Vec2 pb = map.station(b).position;
            const LineAxis edgeAxis =
                classifyVector(Vec2{pb.x - pa.x, pb.y - pa.y});
            uniqueEdgeKeys.push_back(key);
            uniqueEdgeAxes.push_back(static_cast<int>(edgeAxis));
            ++structure.axisEdges[static_cast<int>(edgeAxis)];
        }
    }
    structure.uniqueEdges = static_cast<int>(uniqueEdgeKeys.size());
    structure.axisLineEntropy = axisEntropy(structure.axisLines);
    structure.axisEdgeEntropy = axisEntropy(structure.axisEdges);

    for (int i = 0; i < map.stationCount(); ++i) {
        const int count = lineCountPerStation[static_cast<std::size_t>(i)];
        structure.maxLinesAtStation = std::max(structure.maxLinesAtStation, count);
        if (count == 0) {
            ++structure.stationsUncovered;
            ++structure.lineCountHistogram[0];
        } else {
            if (count == 1) {
                ++structure.stationsWithOneLine;
            } else {
                ++structure.stationsWithMultipleLines;
            }
            ++structure.lineCountHistogram[std::min(4, count)];
        }
    }

    for (std::size_t i = 0; i < lines.size(); ++i) {
        bool duplicateAlreadyCounted = false;
        for (std::size_t k = 0; k < i; ++k) {
            if (sameStationSet(lines[i], lines[k])) {
                duplicateAlreadyCounted = true;
                break;
            }
        }
        if (duplicateAlreadyCounted) {
            continue;
        }
        int groupSize = 1;
        for (std::size_t j = i + 1; j < lines.size(); ++j) {
            if (sameStationSet(lines[i], lines[j])) {
                ++groupSize;
            }
        }
        structure.duplicateExtraLines += groupSize - 1;
    }

    for (std::size_t i = 0; i < lines.size(); ++i) {
        for (std::size_t j = i + 1; j < lines.size(); ++j) {
            const Line& a = lines[i];
            const Line& b = lines[j];
            if (a.stationCount() < 2 || b.stationCount() < 2) {
                continue;
            }
            bool shared = false;
            bool simple = false;
            LineMergeOpportunity best;
            best.firstLine = static_cast<int>(i);
            best.secondLine = static_cast<int>(j);
            best.turnDeg = 180.0;

            for (int aSide = 0; aSide < 2; ++aSide) {
                for (int bSide = 0; bSide < 2; ++bSide) {
                    const Index aEndpoint =
                        aSide == 0 ? a.stations().front() : a.stations().back();
                    const Index bEndpoint =
                        bSide == 0 ? b.stations().front() : b.stations().back();
                    if (aEndpoint != bEndpoint) {
                        continue;
                    }
                    shared = true;
                    DynamicArray<Index> merged;
                    double turn = 180.0;
                    if (!buildMergedPath(map, a, aSide == 0, b, bSide == 1, merged,
                                         turn)) {
                        continue;
                    }
                    simple = true;
                    if (turn < best.turnDeg) {
                        best.turnDeg = turn;
                        best.junction = aEndpoint;
                        best.simple = true;
                    }
                }
            }
            if (!shared) {
                continue;
            }
            ++structure.sharedEndpointPairs;
            if (!simple) {
                continue;
            }
            ++structure.simpleMergePairs;
            if (best.turnDeg <= 45.0) {
                ++structure.straightMergePairs;
            }
            if (best.turnDeg >= 135.0) {
                ++structure.reversalPairs;
            }
            structure.mergeOpportunities.push_back(best);
        }
    }

    return structure;
}

const char* axisName(LineAxis axis) {
    switch (axis) {
        case LineAxis::EastWest:
            return "EW";
        case LineAxis::NorthSouth:
            return "NS";
        case LineAxis::Diagonal:
            return "DIAG";
    }
    return "?";
}

void printLineStructure(const LineStructure& structure) {
    std::cout << "structure: stations=" << structure.stationCount
              << " lines=" << structure.lineCount
              << " short(<= " << structure.shortStationThreshold << ")="
              << structure.shortLines
              << " duplicateExtra=" << structure.duplicateExtraLines << '\n';
    std::cout << "line direction (net displacement): EW="
              << structure.axisLines[static_cast<int>(LineAxis::EastWest)]
              << " NS=" << structure.axisLines[static_cast<int>(LineAxis::NorthSouth)]
              << " DIAG=" << structure.axisLines[static_cast<int>(LineAxis::Diagonal)]
              << " entropy=" << structure.axisLineEntropy << '\n';
    std::cout << "used edge direction: EW="
              << structure.axisEdges[static_cast<int>(LineAxis::EastWest)]
              << " NS=" << structure.axisEdges[static_cast<int>(LineAxis::NorthSouth)]
              << " DIAG=" << structure.axisEdges[static_cast<int>(LineAxis::Diagonal)]
              << " uniqueEdges=" << structure.uniqueEdges
              << " entropy=" << structure.axisEdgeEntropy << '\n';
    std::cout << "stations served by line count: 0="
              << structure.lineCountHistogram[0]
              << " 1=" << structure.lineCountHistogram[1]
              << " 2=" << structure.lineCountHistogram[2]
              << " 3=" << structure.lineCountHistogram[3]
              << " 4+=" << structure.lineCountHistogram[4]
              << " max=" << structure.maxLinesAtStation << '\n';
    std::cout << "merge: sharedEndpointPairs=" << structure.sharedEndpointPairs
              << " simple=" << structure.simpleMergePairs
              << " straight(<=45deg)=" << structure.straightMergePairs
              << " reversals(>=135deg)=" << structure.reversalPairs << '\n';
}

}  // namespace plan
}  // namespace uc
