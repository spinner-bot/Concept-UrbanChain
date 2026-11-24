#pragma once

#include <string>

#include "core/network.h"

namespace uc {
namespace io {

bool loadLines(const std::string& path, Network& network, std::string* error);
bool saveLines(const std::string& path, const Network& network, std::string* error);

}  // namespace io
}  // namespace uc
