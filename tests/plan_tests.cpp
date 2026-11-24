#include <cmath>
#include <cstdio>
#include <string>

#include "core/map.h"
#include "core/network.h"
#include "plan/demand.h"
#include "plan/evaluator.h"
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

}  // namespace

int main() {
    testWeights();
    testDemand();
    testEvaluatorBasic();
    testUnreachablePolicies();
    if (failures == 0) {
        std::printf("all plan tests passed\n");
        return 0;
    }
    std::printf("%d plan test(s) failed\n", failures);
    return 1;
}
