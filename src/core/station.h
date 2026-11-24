#pragma once

#include <string>

#include "core/types.h"

namespace uc {

struct Station {
    std::string id;    // 稳定的外部标识，文件与线路引用使用
    std::string name;  // 展示名称
    Vec2 position;
    int type = 0;      // 站点类型预留：地下 / 地面 / 高架等
};

}  // namespace uc
