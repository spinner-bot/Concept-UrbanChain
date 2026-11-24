#pragma once

#include <cmath>
#include <limits>

namespace uc {

using Index = int;

constexpr Index kInvalidIndex = -1;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

inline double euclideanDistance(const Vec2& a, const Vec2& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace uc
