#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <string>

#include "core/map.h"
#include "core/network.h"
#include "plan/audit.h"
#include "plan/demand.h"
#include "plan/evaluator.h"
#include "plan/search.h"
#include "plan/weights.h"

using namespace uc;
using namespace uc::plan;

namespace {

int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            ++failures;                                                         \
        }                                                                       \
    } while (false)

Station makeStation(const char* id, double x, double y) {
    Station station;
    station.id = id;
    station.name = id;
    station.position = Vec2{x, y};
    return station;
}

DynamicArray<Index> makeStops(std::initializer_list<Index> values) {
    DynamicArray<Index> stops;
    for (Index value : values) {
        stops.push_back(value);
    }
    return stops;
}

Line makeLine(const DynamicArray<Index>& stops) {
    Line line(0, "L", 0xff0000u);
    for (std::size_t i = 0; i < stops.size(); ++i) {
        line.stations().push_back(stops[i]);
    }
    line.ops().dwellSec = 30.0;
    line.ops().headway.push_back(HeadwayPeriod{0, 86400, 300});
    for (std::size_t i = 1; i < stops.size(); ++i) {
        line.ops().segmentRunSec.push_back(120.0);
    }
    return line;
}

DemandResult makeDemand(const DynamicArray<double>& weights, int count, double beta,
                        int clusterCount) {
    DemandResult demand;
    demand.stationCount = count;
    demand.effectiveBeta = beta;
    demand.od.resize(static_cast<std::size_t>(count) * static_cast<std::size_t>(count));
    for (std::size_t i = 0; i < demand.od.size(); ++i) {
        demand.od[i] = 0.0;
    }
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            const double value =
                weights[static_cast<std::size_t>(i)] *
                weights[static_cast<std::size_t>(j)];
            demand.od[static_cast<std::size_t>(i) * count + static_cast<std::size_t>(j)] =
                value;
            demand.od[static_cast<std::size_t>(j) * count + static_cast<std::size_t>(i)] =
                value;
            demand.total += value * 2.0;
        }
    }
    (void)clusterCount;
    return demand;
}

void testWeights() {
    Map map = [] {
        Map value;
        value.addStation(makeStation("A", 0.0, 0.0));
        value.addStation(makeStation("B", 1.0, 0.0));
        value.addStation(makeStation("C", 0.0, 1.0));
        value.addStation(makeStation("D", 1.0, 1.0));
        value.addEdge(0, 1, 1.0);
        value.addEdge(0, 2, 1.0);
        value.addEdge(1, 3, 1.0);
        value.addEdge(2, 3, 1.0);
        return value;
    }();

    WeightParams params;
    params.clusters = 1;
    params.seed = 1;
    const WeightResult single = computeWeights(map, params);
    CHECK(single.clusterCount == 1);
    CHECK(single.weights.size() == 4);
    CHECK(single.restartsTried == 2);  // 最远点 + 自动 1 次随机
    CHECK(single.inertia >= 0.0);
    double sum = 0.0;
    for (std::size_t i = 0; i < single.weights.size(); ++i) {
        CHECK(single.weights[i] > 0.0);
        sum += single.weights[i];
    }
    CHECK(std::fabs(sum / 4.0 - 1.0) < 1e-9);
    CHECK(single.scaleR > 0.0);

    params.clusters = 2;
    const WeightResult two = computeWeights(map, params);
    CHECK(two.clusterCount == 2);
    CHECK(two.weights.size() == 4);
    CHECK(two.clusterOf.size() == 4);
    CHECK(two.restartsTried == 11);  // 最远点 + 自动 10 次随机
}

void testDemand() {
    Map map;
    map.addStation(makeStation("A", 0.0, 0.0));
    map.addStation(makeStation("B", 1.0, 0.0));
    map.addStation(makeStation("C", 2.0, 0.0));

    WeightResult weights;
    weights.weights.push_back(1.0);
    weights.weights.push_back(2.0);
    weights.weights.push_back(4.0);
    weights.clusterCount = 1;
    DemandParams params;
    params.beta = 2.0;
    const DemandResult demand = computeDemand(map, weights, params);
    CHECK(demand.stationCount == 3);
    CHECK(std::fabs(demand.effectiveBeta - 2.0) < 1e-9);
    CHECK(std::fabs(demand.od[0 * 3 + 2] - 1.0) < 1e-9);
    CHECK(std::fabs(demand.od[2 * 3 + 0] - 1.0) < 1e-9);

    weights.clusterCount = 2;
    const DemandResult forced = computeDemand(map, weights, params);
    CHECK(std::fabs(forced.effectiveBeta - 1.0) < 1e-9);
    CHECK(std::fabs(forced.od[0 * 3 + 2] - 2.0) < 1e-9);
}

Map makeTinyGrid(int cols, int rows) {
    Map map;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            map.addStation(makeStation(
                ("S" + std::to_string(r * cols + c)).c_str(), c, r));
        }
    }
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const Index index = r * cols + c;
            if (c + 1 < cols) {
                map.addEdge(index, index + 1, 1.0);
            }
            if (r + 1 < rows) {
                map.addEdge(index, index + cols, 1.0);
            }
        }
    }
    return map;
}

void testGreedyPlan() {
    const Map map = makeTinyGrid(3, 3);
    WeightParams weightParams;
    weightParams.clusters = 2;
    weightParams.seed = 1;
    const WeightResult weights = computeWeights(map, weightParams);
    DemandParams demandParams;
    const DemandResult demand = computeDemand(map, weights, demandParams);

    EvalParams params;
    params.kStation = 0.0;
    params.shortBeta = 10.0;
    params.overrideLineOps = true;
    params.evaluateOd = true;
    params.ops.initialTimeSec = 0.0;
    params.ops.headwaySec = 300;
    params.ops.dwellSec = 30.0;
    params.ops.segmentRunSec = 120.0;
    params.ops.transferSec = 120.0;
    params.connectivity = ConnectivityRequirement::CoveredStations;

    SearchParams search;
    search.maxLines = 4;
    search.topStations = 6;
    search.candidateLimit = 40;

    const PlanResult first = buildGreedyPlan(map, weights, demand, params, search);
    CHECK(first.steps > 0);
    CHECK(!first.lines.empty());
    CHECK(static_cast<int>(first.lines.size()) <= search.maxLines);
    CHECK(first.metrics.feasible);
    CHECK(first.metrics.connected);
    CHECK(first.metrics.r1 > 0.0);
    CHECK(std::isfinite(first.metrics.r3));

    const PlanResult second = buildGreedyPlan(map, weights, demand, params, search);
    CHECK(first.lines.size() == second.lines.size());
    CHECK(std::fabs(first.metrics.r1 - second.metrics.r1) < 1e-9);
}

void testEvaluatorBasic() {
    Map map;
    map.addStation(makeStation("A", 0.0, 0.0));
    map.addStation(makeStation("B", 1.0, 0.0));
    map.addEdge(0, 1, 1.0, 1.0);

    DynamicArray<Line> lines;
    DynamicArray<Index> stops;
    stops.push_back(0);
    stops.push_back(1);
    lines.push_back(makeLine(stops));

    WeightResult weights;
    weights.weights.push_back(1.0);
    weights.weights.push_back(1.0);
    weights.clusterCount = 1;
    const DemandResult demand = makeDemand(weights.weights, 2, 1.0, 1);

    EvalParams params;
    params.kStation = 0.0;
    params.shortBeta = 1.0;
    params.unreachable = UnreachablePolicy::Exclude;
    params.overrideLineOps = true;
    params.ops.initialTimeSec = 0.0;
    params.ops.headwaySec = 300;
    params.ops.dwellSec = 30.0;
    params.ops.segmentRunSec = 120.0;
    params.ops.transferSec = 120.0;

    const Metrics metrics = evaluate(map, lines, weights, demand, params);
    CHECK(metrics.valid);
    CHECK(metrics.feasible);
    CHECK(metrics.connected);
    CHECK(metrics.stationCount == 2);
    CHECK(metrics.lineCount == 1);
    CHECK(std::fabs(metrics.r1 - 1.0) < 1e-9);
    CHECK(std::fabs(metrics.r2 - 1.0) < 1e-9);
    CHECK(std::fabs(metrics.r3 - 300.0) < 1e-6);
    CHECK(std::fabs(metrics.r4 - 0.0) < 1e-9);
    CHECK(std::fabs(metrics.r5 - 1.0) < 1e-9);
    CHECK(std::fabs(metrics.r6 - 0.0) < 1e-9);
    CHECK(metrics.unreachablePairs == 0);
}


void testOdDetourMetric() {
    Map map;
    map.addStation(makeStation("A", 0.0, 0.0));
    map.addStation(makeStation("B", 10.0, 0.0));
    map.addStation(makeStation("C", 0.0, 1.0));
    map.addEdge(0, 1, 1.0, 10.0);
    map.addEdge(1, 2, 1.0, std::sqrt(101.0));

    DynamicArray<Line> lines;
    lines.push_back(makeLine(makeStops({0, 1, 2})));

    WeightResult weights;
    for (int i = 0; i < 3; ++i) {
        weights.weights.push_back(1.0);
    }
    weights.clusterCount = 1;
    const DemandResult demand = makeDemand(weights.weights, 3, 1.0, 1);

    EvalParams params;
    params.kStation = 0.0;
    params.shortBeta = 0.0;
    params.overrideLineOps = true;
    params.ops.initialTimeSec = 0.0;
    params.ops.headwaySec = 120;
    params.ops.dwellSec = 0.0;
    params.ops.segmentRunSec = 1.0;
    params.ops.transferSec = 0.0;
    params.unreachable = UnreachablePolicy::Exclude;

    const Metrics metrics = evaluate(map, lines, weights, demand, params);
    CHECK(metrics.feasible);
    // A→C 必须绕行 A-B-C，路线距离约 20，直距 1；OD 加权后 R5 应远大于 1。
    CHECK(metrics.r5 > 5.0);
}

void testConnectivityGate() {
    Map map;
    map.addStation(makeStation("A", 0.0, 0.0));
    map.addStation(makeStation("B", 1.0, 0.0));
    map.addStation(makeStation("C", 2.0, 0.0));
    map.addStation(makeStation("D", 3.0, 0.0));
    map.addEdge(0, 1, 1.0);
    map.addEdge(1, 2, 1.0);
    map.addEdge(2, 3, 1.0);

    WeightResult weights;
    for (int i = 0; i < 4; ++i) {
        weights.weights.push_back(1.0);
    }
    weights.clusterCount = 1;
    const DemandResult demand = makeDemand(weights.weights, 4, 1.0, 1);

    EvalParams params;
    params.kStation = 0.0;
    params.shortBeta = 10.0;
    params.overrideLineOps = true;
    params.ops.initialTimeSec = 0.0;
    params.ops.headwaySec = 300;
    params.ops.dwellSec = 30.0;
    params.ops.segmentRunSec = 120.0;
    params.ops.transferSec = 120.0;

    DynamicArray<Line> split;
    DynamicArray<Index> ab;
    ab.push_back(0);
    ab.push_back(1);
    split.push_back(makeLine(ab));
    DynamicArray<Index> cd;
    cd.push_back(2);
    cd.push_back(3);
    split.push_back(makeLine(cd));

    params.connectivity = ConnectivityRequirement::AllStations;
    const Metrics disconnected = evaluate(map, split, weights, demand, params);
    CHECK(disconnected.coveredStations == 4);
    CHECK(!disconnected.connected);
    CHECK(!disconnected.feasible);
    CHECK(disconnected.networkComponents >= 2);
    CHECK(std::isinf(disconnected.r3));

    params.connectivity = ConnectivityRequirement::CoveredStations;
    const Metrics coveredOnly = evaluate(map, split, weights, demand, params);
    CHECK(!coveredOnly.connected);
    CHECK(!coveredOnly.feasible);

    split.push_back(makeLine([] {
        DynamicArray<Index> stops;
        stops.push_back(1);
        stops.push_back(2);
        return stops;
    }()));
    params.connectivity = ConnectivityRequirement::AllStations;
    const Metrics connected = evaluate(map, split, weights, demand, params);
    CHECK(connected.connected);
    CHECK(connected.feasible);
    CHECK(std::isfinite(connected.r3));
}

void testCoveredStationsPartialCoverage() {
    Map map;
    map.addStation(makeStation("A", 0.0, 0.0));
    map.addStation(makeStation("B", 1.0, 0.0));
    map.addStation(makeStation("C", 2.0, 0.0));
    map.addStation(makeStation("D", 3.0, 0.0));
    map.addEdge(0, 1, 1.0);
    map.addEdge(1, 2, 1.0);
    map.addEdge(2, 3, 1.0);

    WeightResult weights;
    for (int i = 0; i < 4; ++i) {
        weights.weights.push_back(1.0);
    }
    weights.clusterCount = 1;
    const DemandResult demand = makeDemand(weights.weights, 4, 1.0, 1);

    DynamicArray<Line> lines;
    DynamicArray<Index> stops;
    stops.push_back(0);
    stops.push_back(1);
    stops.push_back(2);
    lines.push_back(makeLine(stops));

    EvalParams params;
    params.kStation = 0.0;
    params.shortBeta = 10.0;
    params.overrideLineOps = true;
    params.ops.initialTimeSec = 0.0;
    params.ops.headwaySec = 300;
    params.ops.dwellSec = 30.0;
    params.ops.segmentRunSec = 120.0;
    params.ops.transferSec = 120.0;
    params.connectivity = ConnectivityRequirement::CoveredStations;
    params.unreachable = UnreachablePolicy::Exclude;

    const Metrics metrics = evaluate(map, lines, weights, demand, params);
    CHECK(metrics.coveredStations == 3);
    CHECK(metrics.connected);
    CHECK(metrics.feasible);
    CHECK(std::fabs(metrics.r1 - 0.75) < 1e-9);
    CHECK(metrics.unreachablePairs == 3);
    CHECK(std::isfinite(metrics.r3));
}

void testUnreachablePolicies() {
    Map map;
    map.addStation(makeStation("A", 0.0, 0.0));
    map.addStation(makeStation("B", 1.0, 0.0));
    map.addStation(makeStation("C", 2.0, 0.0));
    map.addEdge(0, 1, 1.0, 1.0);

    DynamicArray<Line> lines;
    DynamicArray<Index> stops;
    stops.push_back(0);
    stops.push_back(1);
    lines.push_back(makeLine(stops));

    WeightResult weights;
    weights.weights.push_back(1.0);
    weights.weights.push_back(1.0);
    weights.weights.push_back(1.0);
    weights.clusterCount = 1;
    const DemandResult demand = makeDemand(weights.weights, 3, 1.0, 1);

    EvalParams params;
    params.kStation = 0.0;
    params.shortBeta = 10.0;
    params.overrideLineOps = true;
    params.ops.initialTimeSec = 0.0;
    params.ops.headwaySec = 300;
    params.ops.dwellSec = 30.0;
    params.ops.segmentRunSec = 120.0;
    params.ops.transferSec = 120.0;
    params.unreachablePenaltySec = 3600.0;
    params.connectivity = ConnectivityRequirement::None;

    params.unreachable = UnreachablePolicy::Exclude;
    const Metrics excluded = evaluate(map, lines, weights, demand, params);
    CHECK(excluded.unreachablePairs == 2);
    CHECK(excluded.reachablePairs == 1);
    CHECK(std::fabs(excluded.r1 - 2.0 / 3.0) < 1e-9);
    CHECK(std::fabs(excluded.r3 - 300.0) < 1e-6);
    CHECK(std::fabs(excluded.odTotal - 1.0) < 1e-9);

    params.unreachable = UnreachablePolicy::Penalty;
    const Metrics penalty = evaluate(map, lines, weights, demand, params);
    CHECK(penalty.unreachablePairs == 2);
    CHECK(std::fabs(penalty.r3 - 2500.0) < 1e-6);
    CHECK(std::fabs(penalty.odTotal - 3.0) < 1e-9);

    params.unreachable = UnreachablePolicy::Infinite;
    const Metrics infinite = evaluate(map, lines, weights, demand, params);
    CHECK(std::isinf(infinite.r3));
    CHECK(std::isinf(infinite.r4));
}

void testCoverageModes() {
    const EvalParams defaults;
    CHECK(defaults.coverageMode == CoverageMode::MultiLineSaturating);
    CHECK(std::fabs(defaults.multiLineLambda - 0.25) < 1e-12);

    Map map;
    map.addStation(makeStation("A", 0.0, 0.0));
    map.addStation(makeStation("B", 1.0, 0.0));
    map.addStation(makeStation("C", 2.0, 0.0));
    map.addEdge(0, 1, 1.0);
    map.addEdge(1, 2, 1.0);

    DynamicArray<Line> lines;
    lines.push_back(makeLine(makeStops({0, 1, 2})));
    lines.push_back(makeLine(makeStops({0, 1})));

    WeightResult weights;
    for (int i = 0; i < 3; ++i) {
        weights.weights.push_back(1.0);
    }
    weights.clusterCount = 1;
    const DemandResult demand = makeDemand(weights.weights, 3, 1.0, 1);

    EvalParams params;
    params.kStation = 0.0;
    params.shortBeta = 10.0;
    params.overrideLineOps = true;
    params.ops.initialTimeSec = 0.0;
    params.ops.headwaySec = 300;
    params.ops.dwellSec = 30.0;
    params.ops.segmentRunSec = 120.0;
    params.ops.transferSec = 120.0;
    params.connectivity = ConnectivityRequirement::CoveredStations;
    params.unreachable = UnreachablePolicy::Exclude;
    params.coverageMode = CoverageMode::Unique;

    const Metrics unique = evaluate(map, lines, weights, demand, params);
    CHECK(std::fabs(unique.r1 - 1.0) < 1e-9);
    CHECK(unique.coveredStations == 3);

    params.coverageMode = CoverageMode::MultiLineLog;
    params.multiLineLambda = 0.25;
    const Metrics logMetrics = evaluate(map, lines, weights, demand, params);
    CHECK(logMetrics.r1 > 1.0);
    CHECK(std::fabs(logMetrics.r1 - (2.0 * (1.0 + 0.25 * std::log(2.0)) + 1.0) /
                                       3.0) < 1e-9);
    CHECK(logMetrics.coveredStations == 3);

    params.coverageMode = CoverageMode::MultiLineSaturating;
    const Metrics satMetrics = evaluate(map, lines, weights, demand, params);
    CHECK(satMetrics.r1 > 1.0);
    CHECK(std::fabs(satMetrics.r1 - (2.0 * 1.125 + 1.0) / 3.0) < 1e-9);
    CHECK(satMetrics.coveredStations == 3);
}


void testLineSimilarityPenalty() {
    Map map;
    for (int i = 0; i < 5; ++i) {
        map.addStation(makeStation(("S" + std::to_string(i)).c_str(), i * 10.0, 0.0));
    }
    for (int i = 0; i + 1 < 5; ++i) {
        map.addEdge(i, i + 1, 1.0, 10.0);
    }

    ScoreParams score;
    score.lineProximityDistance = 1.0;

    DynamicArray<Line> duplicated;
    duplicated.push_back(makeLine(makeStops({0, 1, 2, 3})));
    duplicated.push_back(makeLine(makeStops({0, 1, 2, 3})));
    const double duplicatePenalty = lineSimilarityPenalty(map, duplicated, score);
    CHECK(duplicatePenalty > 0.12);
    CHECK(duplicatePenalty < 0.14);

    DynamicArray<Line> distinct;
    distinct.push_back(makeLine(makeStops({0, 1, 2})));
    distinct.push_back(makeLine(makeStops({2, 3, 4})));
    const double distinctPenalty = lineSimilarityPenalty(map, distinct, score);
    CHECK(distinctPenalty < duplicatePenalty);

    DynamicArray<Line> single;
    single.push_back(makeLine(makeStops({0, 1, 2})));
    CHECK(lineSimilarityPenalty(map, single, score) == 0.0);
}

void testNearestEdgeAudit() {
    Map map;
    for (int i = 0; i < 10; ++i) {
        map.addStation(makeStation(("S" + std::to_string(i)).c_str(), i * 10.0, 0.0));
    }
    for (int i = 0; i + 1 < 10; ++i) {
        map.addEdge(i, i + 1, 1.0, 10.0);
    }
    // 10 站要求 X = 2；端点额外连到第二近站。
    map.addEdge(0, 2, 1.0, 20.0);
    map.addEdge(7, 9, 1.0, 20.0);

    NearestEdgeReport report = auditNearestEdges(map);
    CHECK(report.required == 2);
    CHECK(report.violatedStations == 0);
    CHECK(report.missingEdges == 0);

    CHECK(map.removeEdge(0, 2));
    report = auditNearestEdges(map);
    CHECK(report.violatedStations >= 1);
    CHECK(report.missingEdges >= 1);
}

void testMapGeometryAudit() {
    Map map;
    map.addStation(makeStation("A", 0.0, 0.0));
    map.addStation(makeStation("B", 1.0, 0.0));
    map.addStation(makeStation("C", 2.0, 0.0));
    map.addEdge(0, 1, 1.0, 10.0);  // 几何长度 1，存储长度 10
    map.addEdge(1, 2, 1.0, 1.0);

    const MapGeometryReport report = auditMapGeometry(map);
    CHECK(report.edgeCount == 2);
    CHECK(report.mismatchedEdges == 1);
    CHECK(report.issues.size() == 1);
    CHECK(report.issues[0].from == 0);
    CHECK(report.issues[0].to == 1);
    CHECK(std::fabs(report.issues[0].geometricLength - 1.0) < 1e-9);
    CHECK(std::fabs(report.issues[0].storedLength - 10.0) < 1e-9);
    CHECK(report.worstRatio > 9.9);
}

void testLineAudit() {
    Map map;
    for (int i = 0; i < 4; ++i) {
        map.addStation(makeStation(("S" + std::to_string(i)).c_str(), i, 0));
    }
    for (int i = 0; i + 1 < 4; ++i) {
        map.addEdge(i, i + 1, 1.0);
    }

    DynamicArray<Line> lines;
    lines.push_back(makeLine(makeStops({0, 1, 2})));
    lines.push_back(makeLine(makeStops({2, 3})));
    lines.push_back(makeLine(makeStops({0, 1})));
    lines.push_back(makeLine(makeStops({0, 1, 2})));  // 与第一条重复

    const LineStructure report = auditLines(map, lines, 3);
    CHECK(report.lineCount == 4);
    CHECK(report.shortLines == 4);
    CHECK(report.duplicateExtraLines == 1);
    CHECK(report.axisLines[static_cast<int>(LineAxis::EastWest)] == 4);
    CHECK(report.axisEdges[static_cast<int>(LineAxis::EastWest)] == 3);
    CHECK(report.straightMergePairs >= 1);
    CHECK(report.maxLinesAtStation >= 3);

    Line merged;
    double turn = 180.0;
    CHECK(mergeLinePair(map, lines[0], lines[1], merged, &turn));
    CHECK(merged.stationCount() == 4);
    CHECK(merged.stations().front() == 0);
    CHECK(merged.stations().back() == 3);
    CHECK(turn < 1e-6);

    Line duplicateMerge;
    CHECK(!mergeLinePair(map, lines[0], lines[3], duplicateMerge, nullptr));
}

void testGridAuditBalance() {
    const Map map = makeTinyGrid(3, 3);
    DynamicArray<Line> lines;
    for (int r = 0; r < 3; ++r) {
        lines.push_back(makeLine(makeStops({r * 3, r * 3 + 1, r * 3 + 2})));
    }
    for (int c = 0; c < 3; ++c) {
        lines.push_back(makeLine(makeStops({c, c + 3, c + 6})));
    }
    const LineStructure report = auditLines(map, lines, 2);
    CHECK(report.axisLines[static_cast<int>(LineAxis::EastWest)] == 3);
    CHECK(report.axisLines[static_cast<int>(LineAxis::NorthSouth)] == 3);
    CHECK(report.axisEdges[static_cast<int>(LineAxis::EastWest)] == 6);
    CHECK(report.axisEdges[static_cast<int>(LineAxis::NorthSouth)] == 6);
    CHECK(report.axisLineEntropy > 0.999);
    CHECK(report.axisEdgeEntropy > 0.999);
    CHECK(report.shortLines == 0);
    CHECK(report.duplicateExtraLines == 0);
}

}  // namespace

int main() {
    testWeights();
    testDemand();
    testGreedyPlan();
    testEvaluatorBasic();
    testOdDetourMetric();
    testConnectivityGate();
    testCoveredStationsPartialCoverage();
    testUnreachablePolicies();
    testCoverageModes();
    testMapGeometryAudit();
    testLineSimilarityPenalty();
    testNearestEdgeAudit();
    testLineAudit();
    testGridAuditBalance();
    if (failures == 0) {
        std::printf("all plan tests passed\n");
        return 0;
    }
    std::printf("%d plan test(s) failed\n", failures);
    return 1;
}
