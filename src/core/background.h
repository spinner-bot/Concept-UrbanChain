#pragma once

#include <string>

#include "core/types.h"
#include "ds/dynamic_array.h"

namespace uc {

enum class BackgroundKind {
    Boundary,
    River,
    Coast,
    Mountain,
    Region,
};

struct BackgroundFeature {
    std::string id;
    BackgroundKind kind = BackgroundKind::Boundary;
    bool closed = false;
    DynamicArray<Vec2> points;
};

inline const char* backgroundKindName(BackgroundKind kind) {
    switch (kind) {
        case BackgroundKind::Boundary:
            return "boundary";
        case BackgroundKind::River:
            return "river";
        case BackgroundKind::Coast:
            return "coast";
        case BackgroundKind::Mountain:
            return "mountain";
        case BackgroundKind::Region:
            return "region";
    }
    return "boundary";
}

inline bool parseBackgroundKind(const std::string& text, BackgroundKind* kind) {
    if (text == "boundary") {
        *kind = BackgroundKind::Boundary;
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
    if (text == "mountain") {
        *kind = BackgroundKind::Mountain;
        return true;
    }
    if (text == "region") {
        *kind = BackgroundKind::Region;
        return true;
    }
    return false;
}

}  // namespace uc
