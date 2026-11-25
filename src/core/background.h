#pragma once

#include <string>

#include "core/types.h"
#include "ds/dynamic_array.h"

namespace uc {

enum class BackgroundKind {
    Region,
    Water,
    Park,
    Hospital,
    Road,
    River,
    Coast,
    Boundary,
    Mountain,
};

struct BackgroundFeature {
    std::string id;
    BackgroundKind kind = BackgroundKind::Boundary;
    bool closed = false;
    // 线性要素（河流/道路）在数据坐标中的宽度；面状要素忽略。
    double width = 0.0;
    DynamicArray<Vec2> points;
};

inline const char* backgroundKindName(BackgroundKind kind) {
    switch (kind) {
        case BackgroundKind::Region:
            return "region";
        case BackgroundKind::Water:
            return "water";
        case BackgroundKind::Park:
            return "park";
        case BackgroundKind::Hospital:
            return "hospital";
        case BackgroundKind::Road:
            return "road";
        case BackgroundKind::River:
            return "river";
        case BackgroundKind::Coast:
            return "coast";
        case BackgroundKind::Boundary:
            return "boundary";
        case BackgroundKind::Mountain:
            return "mountain";
    }
    return "boundary";
}

inline bool parseBackgroundKind(const std::string& text, BackgroundKind* kind) {
    if (text == "region") {
        *kind = BackgroundKind::Region;
        return true;
    }
    if (text == "water") {
        *kind = BackgroundKind::Water;
        return true;
    }
    if (text == "park") {
        *kind = BackgroundKind::Park;
        return true;
    }
    if (text == "hospital") {
        *kind = BackgroundKind::Hospital;
        return true;
    }
    if (text == "road") {
        *kind = BackgroundKind::Road;
        return true;
    }
    if (text == "river") {
        *kind = BackgroundKind::River;
        return true;
    }
    if (text == "coast") {
        *kind = BackgroundKind::Coast;
        return true;
    }
    if (text == "boundary") {
        *kind = BackgroundKind::Boundary;
        return true;
    }
    if (text == "mountain") {
        *kind = BackgroundKind::Mountain;
        return true;
    }
    return false;
}

}  // namespace uc
