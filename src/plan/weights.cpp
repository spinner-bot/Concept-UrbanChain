#include "plan/weights.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <utility>

namespace uc {
namespace plan {
namespace {

struct KMeansOutcome {
    DynamicArray<Vec2> centers;
    DynamicArray<int> clusterOf;
    double inertia = kInfinity;
    int iterations = 0;
};

double squaredDistance(const Vec2& a, const Vec2& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

DynamicArray<Vec2> farthestFirstInit(const Map& map, int K) {
    const int count = map.stationCount();
    DynamicArray<Vec2> centers;
    if (count <= 0 || K <= 0) {
        return centers;
    }

    Vec2 mean;
    for (int i = 0; i < count; ++i) {
        mean.x += map.station(i).position.x;
        mean.y += map.station(i).position.y;
    }
    mean.x /= count;
    mean.y /= count;

    int first = 0;
    double best = -1.0;
    for (int i = 0; i < count; ++i) {
        const double d = squaredDistance(map.station(i).position, mean);
        if (d > best) {
            best = d;
            first = i;
        }
    }
    centers.push_back(map.station(first).position);

    while (static_cast<int>(centers.size()) < K) {
        int chosen = 0;
        double chosenDistance = -1.0;
        for (int i = 0; i < count; ++i) {
            double nearest = kInfinity;
            for (std::size_t k = 0; k < centers.size(); ++k) {
                nearest = std::min(nearest, squaredDistance(map.station(i).position, centers[k]));
            }
            if (nearest > chosenDistance) {
                chosenDistance = nearest;
                chosen = i;
            }
        }
        centers.push_back(map.station(chosen).position);
    }
    return centers;
}

DynamicArray<Vec2> randomInit(const Map& map, int K, std::uint32_t seed) {
    const int count = map.stationCount();
    DynamicArray<Vec2> centers;
    if (count <= 0 || K <= 0) {
        return centers;
    }
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> pick(0, count - 1);
    DynamicArray<unsigned char> used;
    used.resize(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        used[static_cast<std::size_t>(i)] = 0;
    }
    while (static_cast<int>(centers.size()) < K) {
        const int index = pick(rng);
        if (used[static_cast<std::size_t>(index)]) {
            continue;
        }
        used[static_cast<std::size_t>(index)] = 1;
        centers.push_back(map.station(index).position);
    }
    return centers;
}

KMeansOutcome runKMeans(const Map& map, int K, DynamicArray<Vec2> centers) {
    KMeansOutcome outcome;
    const int count = map.stationCount();
    outcome.centers = static_cast<DynamicArray<Vec2>&&>(centers);
    outcome.clusterOf.resize(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        outcome.clusterOf[static_cast<std::size_t>(i)] = 0;
    }

    int iteration = 0;
    for (; iteration < 100; ++iteration) {
        bool changed = false;
        for (int i = 0; i < count; ++i) {
            const Vec2& position = map.station(i).position;
            int best = 0;
            double bestDistance = kInfinity;
            for (int k = 0; k < K; ++k) {
                const double d =
                    squaredDistance(position, outcome.centers[static_cast<std::size_t>(k)]);
                if (d < bestDistance) {
                    bestDistance = d;
                    best = k;
                }
            }
            if (outcome.clusterOf[static_cast<std::size_t>(i)] != best) {
                outcome.clusterOf[static_cast<std::size_t>(i)] = best;
                changed = true;
            }
        }

        DynamicArray<double> sumX;
        DynamicArray<double> sumY;
        DynamicArray<int> counts;
        sumX.resize(static_cast<std::size_t>(K));
        sumY.resize(static_cast<std::size_t>(K));
        counts.resize(static_cast<std::size_t>(K));
        for (int k = 0; k < K; ++k) {
            sumX[static_cast<std::size_t>(k)] = 0.0;
            sumY[static_cast<std::size_t>(k)] = 0.0;
            counts[static_cast<std::size_t>(k)] = 0;
        }
        for (int i = 0; i < count; ++i) {
            const int k = outcome.clusterOf[static_cast<std::size_t>(i)];
            sumX[static_cast<std::size_t>(k)] += map.station(i).position.x;
            sumY[static_cast<std::size_t>(k)] += map.station(i).position.y;
            ++counts[static_cast<std::size_t>(k)];
        }
        for (int k = 0; k < K; ++k) {
            if (counts[static_cast<std::size_t>(k)] > 0) {
                outcome.centers[static_cast<std::size_t>(k)] =
                    Vec2{sumX[static_cast<std::size_t>(k)] / counts[static_cast<std::size_t>(k)],
                         sumY[static_cast<std::size_t>(k)] / counts[static_cast<std::size_t>(k)]};
            } else {
                int farthest = 0;
                double farthestDistance = -1.0;
                for (int i = 0; i < count; ++i) {
                    double nearest = kInfinity;
                    for (int other = 0; other < K; ++other) {
                        if (counts[static_cast<std::size_t>(other)] == 0) {
                            continue;
                        }
                        nearest = std::min(
                            nearest,
                            squaredDistance(map.station(i).position,
                                            outcome.centers[static_cast<std::size_t>(other)]));
                    }
                    if (nearest > farthestDistance) {
                        farthestDistance = nearest;
                        farthest = i;
                    }
                }
                outcome.centers[static_cast<std::size_t>(k)] = map.station(farthest).position;
                changed = true;
            }
        }
        if (!changed) {
            break;
        }
    }
    outcome.iterations = iteration + 1;

    for (int i = 0; i < count; ++i) {
        const Vec2& position = map.station(i).position;
        int best = 0;
        double bestDistance = kInfinity;
        for (int k = 0; k < K; ++k) {
            const double d =
                squaredDistance(position, outcome.centers[static_cast<std::size_t>(k)]);
            if (d < bestDistance) {
                bestDistance = d;
                best = k;
            }
        }
        outcome.clusterOf[static_cast<std::size_t>(i)] = best;
    }

    outcome.inertia = 0.0;
    for (int i = 0; i < count; ++i) {
        outcome.inertia += squaredDistance(
            map.station(i).position,
            outcome.centers[static_cast<std::size_t>(
                outcome.clusterOf[static_cast<std::size_t>(i)])]);
    }
    return outcome;
}

int effectiveRestarts(const WeightParams& params, int K) {
    if (params.randomRestarts > 0) {
        return params.randomRestarts;
    }
    return std::max(0, 10 * K - 10) + (K == 1 ? 1 : 0);
}

}  // namespace

WeightResult computeWeights(const Map& map, const WeightParams& params) {
    WeightResult result;
    const int count = map.stationCount();
    if (count <= 0) {
        return result;
    }

    const int K = std::max(1, std::min(params.clusters, count));
    result.clusterCount = K;

    // 候选 0：最远点优先；候选 1..N：固定 seed 的随机初始化。
    // 全部跑完后按 K-means inertia 取最优。
    KMeansOutcome best = runKMeans(map, K, farthestFirstInit(map, K));
    const int restarts = effectiveRestarts(params, K);
    result.restartsTried = 1;
    for (int r = 0; r < restarts; ++r) {
        const std::uint32_t seed =
            static_cast<std::uint32_t>(params.seed) * 1000003u +
            static_cast<std::uint32_t>(r) * 7919u + 17u;
        KMeansOutcome candidate = runKMeans(map, K, randomInit(map, K, seed));
        ++result.restartsTried;
        if (candidate.inertia < best.inertia) {
            best = static_cast<KMeansOutcome&&>(candidate);
        }
    }

    result.clusterOf = best.clusterOf;
    result.centers = best.centers;
    result.iterations = best.iterations;
    result.inertia = best.inertia;

    switch (params.globalCenterMode) {
        case GlobalCenterMode::StationMean: {
            Vec2 sum;
            for (int i = 0; i < count; ++i) {
                sum.x += map.station(i).position.x;
                sum.y += map.station(i).position.y;
            }
            result.globalCenter = Vec2{sum.x / count, sum.y / count};
            break;
        }
        case GlobalCenterMode::ClusterMean: {
            Vec2 sum;
            for (int k = 0; k < K; ++k) {
                sum.x += result.centers[static_cast<std::size_t>(k)].x;
                sum.y += result.centers[static_cast<std::size_t>(k)].y;
            }
            result.globalCenter = Vec2{sum.x / K, sum.y / K};
            break;
        }
        case GlobalCenterMode::Custom:
            result.globalCenter = params.customCenter;
            break;
    }

    double scaleSum = 0.0;
    for (int i = 0; i < count; ++i) {
        scaleSum += euclideanDistance(map.station(i).position, result.globalCenter);
    }
    result.scaleR = scaleSum / count;
    if (result.scaleR <= 1e-12) {
        result.scaleR = 1.0;
    }

    result.weights.resize(static_cast<std::size_t>(count));
    const double epsilon = std::max(params.epsilon, 1e-12);
    const double exponent = std::max(0.0, params.regionalExponent);
    double weightSum = 0.0;
    for (int i = 0; i < count; ++i) {
        const int cluster = result.clusterOf[static_cast<std::size_t>(i)];
        const double d1 =
            euclideanDistance(map.station(i).position,
                              result.centers[static_cast<std::size_t>(cluster)]) /
            result.scaleR;
        const double d2 =
            euclideanDistance(result.centers[static_cast<std::size_t>(cluster)],
                              result.globalCenter) /
            result.scaleR;
        const double weight =
            std::pow(d1 + epsilon, -1.0) * std::pow(d2 + epsilon, -exponent);
        result.weights[static_cast<std::size_t>(i)] = weight;
        weightSum += weight;
    }

    if (params.normalization == WeightNormalization::Mean && count > 0) {
        const double mean = weightSum / count;
        if (mean > 1e-12) {
            for (int i = 0; i < count; ++i) {
                result.weights[static_cast<std::size_t>(i)] /= mean;
            }
        }
    } else if (params.normalization == WeightNormalization::Sum) {
        if (weightSum > 1e-12) {
            for (int i = 0; i < count; ++i) {
                result.weights[static_cast<std::size_t>(i)] /= weightSum;
            }
        }
    }
    return result;
}

}  // namespace plan
}  // namespace uc
