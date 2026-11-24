#pragma once

#include <string>

#include "core/map.h"

namespace uc {
namespace io {

bool loadMap(const std::string& path, Map& map, std::string* error);
bool saveMap(const std::string& path, const Map& map, std::string* error);

}  // namespace io
}  // namespace uc
