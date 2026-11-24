#pragma once

#include <QString>

namespace uc {

enum class Language {
    Chinese,
    English,
};

Language currentLanguage();

// persist=true 时写入 QSettings；测试中传 false，避免污染用户设置。
void setLanguage(Language language, bool persist = true);
void initializeLanguage();

// 以英文原文为键取当前语言文本；未登记时返回英文原文。
QString L(const char* english);

// 将核心层返回的英文错误信息本地化（支持固定条目和少量前缀模式）。
QString localizeMessage(const QString& english);

}  // namespace uc
