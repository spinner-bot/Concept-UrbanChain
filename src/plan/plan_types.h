#pragma once

#include "core/types.h"
#include "ds/dynamic_array.h"

namespace uc {
namespace plan {

enum class GlobalCenterMode {
    StationMean,
    ClusterMean,
    Custom,
};

enum class WeightNormalization {
    Mean,
    Sum,
};

enum class UnreachablePolicy {
    Exclude,
    Penalty,
    Infinite,
};

enum class ConnectivityRequirement {
    None,
    CoveredStations,
    AllStations,
};

struct WeightParams {
    int clusters = 3;
    double regionalExponent = 1.0;
    double epsilon = 1e-3;
    GlobalCenterMode globalCenterMode = GlobalCenterMode::StationMean;
    Vec2 customCenter;
    WeightNormalization normalization = WeightNormalization::Mean;
    int seed = 1;
    // 随机重启次数；<=0 表示自动取 10*K-10+(K==1)。
    // 最远点优先初始化总会额外跑一次，与这些随机种子共同比较 inertia。
    int randomRestarts = 0;
};

struct DemandParams {
    double beta = 1.0;
    bool forceBetaOneWhenMultipleClusters = true;
    bool normalizeTotal = false;
};

struct OpsDefaults {
    double segmentRunSec = 120.0;
    double dwellSec = 30.0;
    int headwaySec = 300;
    double initialTimeSec = 8.0 * 3600.0;
    double transferSec = 120.0;
};

struct EvalParams {
    double kStation = 1.0;
    double shortAlpha = 1.0;
    double shortBeta = 2.0;
    ConnectivityRequirement connectivity = ConnectivityRequirement::CoveredStations;
    UnreachablePolicy unreachable = UnreachablePolicy::Exclude;
    double unreachablePenaltySec = 3600.0;
    bool overrideLineOps = true;
    OpsDefaults ops;
};

struct WeightResult {
    DynamicArray<double> weights;
    DynamicArray<int> clusterOf;
    DynamicArray<Vec2> centers;
    Vec2 globalCenter;
    double scaleR = 1.0;
    int clusterCount = 0;
    int iterations = 0;
    int restartsTried = 0;
    double inertia = 0.0;
};

struct DemandResult {
    DynamicArray<double> od;  // 行优先 n*n，对称
    int stationCount = 0;
    double total = 0.0;
    double effectiveBeta = 1.0;
};

struct Metrics {
    bool valid = true;
    bool feasible = true;
    bool connected = true;
    int networkComponents = 0;
    int coveredStations = 0;
    double r1 = 0.0;
    double r2 = 0.0;
    double r3 = 0.0;
    double r4 = 0.0;
    double r5 = 0.0;
    double r6 = 0.0;

    int stationCount = 0;
    int lineCount = 0;
    int odPairs = 0;
    int reachablePairs = 0;
    int unreachablePairs = 0;
    double odTotal = 0.0;
    double elapsedMs = 0.0;
};

}  // namespace plan
}  // namespace uc
