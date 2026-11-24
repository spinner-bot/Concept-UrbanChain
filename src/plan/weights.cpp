#include "plan/weights.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

namespace uc {
namespace plan {
namespace {

struct XY {
    double x = 0.0;
    double y = 0.0;
};

double distance(const XY& a, const XY& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
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
    result.clusterOf.resize(static_cast<std::size_t>(count));
    result.weights.resize(static_cast<std::size_t>(count));
    result.centers.resize(static_cast<std::size_t>(K));

    std::mt19937 rng(static_cast<std::uint32_t>(params.seed));
    std::uniform_int_distribution<int> pick(0, count - 1);
    DynamicArray<unsigned char> used;
    used.resize(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        used[static_cast<std::size_t>(i)] = 0;
    }

    for (int k = 0; k < K; ++k) {
        int index = pick(rng);
        while (used[static_cast<std::size_t>(index)]) {
            index = pick(rng);
        }
        used[static_cast<std::size_t>(index)] = 1;
        const Vec2& position = map.station(index).position;
        result.centers[static_cast<std::size_t>(k)] = position;
    }

    int iteration = 0;
    for (; iteration < 100; ++iteration) {
        bool changed = false;
        for (int i = 0; i < count; ++i) {
            const Vec2& position = map.station(i).position;
            int best = 0;
            double bestDistance = kInfinity;
            for (int k = 0; k < K; ++k) {
                const double d = euclideanDistance(position, result.centers[static_cast<std::size_t>(k)]);
                if (d < bestDistance) {
                    bestDistance = d;
                    best = k;
                }
            }
            if (result.clusterOf[static_cast<std::size_t>(i)] != best) {
                result.clusterOf[static_cast<std::size_t>(i)] = best;
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
            const int k = result.clusterOf[static_cast<std::size_t>(i)];
            const Vec2& position = map.station(i).position;
            sumX[static_cast<std::size_t>(k)] += position.x;
            sumY[static_cast<std::size_t>(k)] += position.y;
            ++counts[static_cast<std::size_t>(k)];
        }
        for (int k = 0; k < K; ++k) {
            if (counts[static_cast<std::size_t>(k)] > 0) {
                result.centers[static_cast<std::size_t>(k)] =
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
                            euclideanDistance(map.station(i).position,
                                              result.centers[static_cast<std::size_t>(other)]));
                    }
                    if (nearest > farthestDistance) {
                        farthestDistance = nearest;
                        farthest = i;
                    }
                }
                result.centers[static_cast<std::size_t>(k)] = map.station(farthest).position;
                changed = true;
            }
        }
        if (!changed) {
            break;
        }
    }
    result.iterations = iteration + 1;

    // 最终再分配一次，保证 clusterOf 与当前中心一致。
    for (int i = 0; i < count; ++i) {
        const Vec2& position = map.station(i).position;
        int best = 0;
        double bestDistance = kInfinity;
        for (int k = 0; k < K; ++k) {
            const double d = euclideanDistance(position, result.centers[static_cast<std::size_t>(k)]);
            if (d < bestDistance) {
                bestDistance = d;
                best = k;
            }
        }
        result.clusterOf[static_cast<std::size_t>(i)] = best;
    }

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
