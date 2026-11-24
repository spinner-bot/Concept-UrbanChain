#include "core/spline.h"

#include <algorithm>
#include <cmath>

namespace uc {
namespace {

double lerp(double a, double b, double t) { return a + (b - a) * t; }

Vec2 pointOnSegment(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3,
                    double t0, double t1, double t2, double t3, double t) {
    const double a1t = (t - t0) / std::max(t1 - t0, 1e-12);
    const double a2t = (t - t1) / std::max(t2 - t1, 1e-12);
    const double a3t = (t - t2) / std::max(t3 - t2, 1e-12);

    const double a1x = lerp(p0.x, p1.x, a1t);
    const double a1y = lerp(p0.y, p1.y, a1t);
    const double a2x = lerp(p1.x, p2.x, a2t);
    const double a2y = lerp(p1.y, p2.y, a2t);
    const double a3x = lerp(p2.x, p3.x, a3t);
    const double a3y = lerp(p2.y, p3.y, a3t);

    const double b1t = (t - t0) / std::max(t2 - t0, 1e-12);
    const double b2t = (t - t1) / std::max(t3 - t1, 1e-12);
    const double b1x = lerp(a1x, a2x, b1t);
    const double b1y = lerp(a1y, a2y, b1t);
    const double b2x = lerp(a2x, a3x, b2t);
    const double b2y = lerp(a2y, a3y, b2t);

    const double ct = (t - t1) / std::max(t2 - t1, 1e-12);
    return Vec2{lerp(b1x, b2x, ct), lerp(b1y, b2y, ct)};
}

}  // namespace

DynamicArray<Vec2> catmullRomSpline(const DynamicArray<Vec2>& points,
                                    int samplesPerSegment, double alpha) {
    DynamicArray<Vec2> result;
    if (points.size() < 2) {
        for (std::size_t i = 0; i < points.size(); ++i) {
            result.push_back(points[i]);
        }
        return result;
    }
    if (samplesPerSegment < 1) {
        samplesPerSegment = 1;
    }

    if (points.size() == 2) {
        for (int i = 0; i <= samplesPerSegment; ++i) {
            const double t = static_cast<double>(i) / samplesPerSegment;
            result.push_back(Vec2{lerp(points[0].x, points[1].x, t),
                                  lerp(points[0].y, points[1].y, t)});
        }
        return result;
    }

    DynamicArray<Vec2> augmented;
    augmented.push_back(Vec2{2.0 * points[0].x - points[1].x,
                             2.0 * points[0].y - points[1].y});
    for (std::size_t i = 0; i < points.size(); ++i) {
        augmented.push_back(points[i]);
    }
    const std::size_t last = points.size() - 1;
    augmented.push_back(Vec2{2.0 * points[last].x - points[last - 1].x,
                             2.0 * points[last].y - points[last - 1].y});

    DynamicArray<double> knots;
    knots.resize(augmented.size());
    knots[0] = 0.0;
    for (std::size_t i = 1; i < augmented.size(); ++i) {
        const double dx = augmented[i].x - augmented[i - 1].x;
        const double dy = augmented[i].y - augmented[i - 1].y;
        const double distance = std::sqrt(dx * dx + dy * dy);
        knots[i] = knots[i - 1] + std::pow(std::max(distance, 1e-10), alpha);
    }

    result.push_back(points[0]);
    for (std::size_t i = 1; i + 2 < augmented.size(); ++i) {
        const Vec2& p0 = augmented[i - 1];
        const Vec2& p1 = augmented[i];
        const Vec2& p2 = augmented[i + 1];
        const Vec2& p3 = augmented[i + 2];
        const double t0 = knots[i - 1];
        const double t1 = knots[i];
        const double t2 = knots[i + 1];
        const double t3 = knots[i + 2];
        for (int k = 1; k <= samplesPerSegment; ++k) {
            const double t = t1 + (t2 - t1) * static_cast<double>(k) / samplesPerSegment;
            result.push_back(pointOnSegment(p0, p1, p2, p3, t0, t1, t2, t3, t));
        }
    }
    result.push_back(points[last]);
    return result;
}

}  // namespace uc
