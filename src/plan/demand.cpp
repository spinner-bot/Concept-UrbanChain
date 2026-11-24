#include "plan/demand.h"

#include <algorithm>
#include <cmath>

namespace uc {
namespace plan {

DemandResult computeDemand(const Map& map, const WeightResult& weights,
                           const DemandParams& params) {
    DemandResult result;
    const int count = map.stationCount();
    result.stationCount = count;
    result.od.resize(static_cast<std::size_t>(count) * static_cast<std::size_t>(count));
    for (std::size_t i = 0; i < result.od.size(); ++i) {
        result.od[i] = 0.0;
    }
    if (count < 2 || static_cast<int>(weights.weights.size()) != count) {
        return result;
    }

    const bool multiCluster = weights.clusterCount > 1;
    result.effectiveBeta =
        (multiCluster && params.forceBetaOneWhenMultipleClusters)
            ? 1.0
            : std::max(0.0, params.beta);

    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            double distance =
                euclideanDistance(map.station(i).position, map.station(j).position);
            distance = std::max(distance, 1e-9);
            const double value =
                weights.weights[static_cast<std::size_t>(i)] *
                weights.weights[static_cast<std::size_t>(j)] /
                std::pow(distance, result.effectiveBeta);
            const std::size_t forward =
                static_cast<std::size_t>(i) * count + static_cast<std::size_t>(j);
            const std::size_t backward =
                static_cast<std::size_t>(j) * count + static_cast<std::size_t>(i);
            result.od[forward] = value;
            result.od[backward] = value;
            result.total += value * 2.0;
        }
    }

    if (params.normalizeTotal && result.total > 1e-12) {
        for (std::size_t i = 0; i < result.od.size(); ++i) {
            result.od[i] /= result.total;
        }
        result.total = 1.0;
    }
    return result;
}

}  // namespace plan
}  // namespace uc
