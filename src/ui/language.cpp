#include "ui/language.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QStringList>
#include <QTranslator>

#include <cstring>

namespace uc {
namespace {

Language g_language = Language::English;
QTranslator* g_translator = nullptr;

struct Entry {
    const char* english;
    const char* chinese;
};

const Entry kEntries[] = {
    // 菜单 / 动作 / 工具栏
    {"&File", "文件(&F)"},
    {"&Edit", "编辑(&E)"},
    {"&Mode", "模式(&M)"},
    {"&Line", "线路(&L)"},
    {"&View", "视图(&V)"},
    {"&Settings", "设置(&S)"},
    {"&Language", "语言(&L)"},
    {"&Help", "帮助(&H)"},
    {"&New", "新建(&N)"},
    {"&Open...", "打开(&O)..."},
    {"&Save", "保存(&S)"},
    {"Save &As...", "另存为(&A)..."},
    {"E&xit", "退出(&X)"},
    {"&Undo", "撤销(&U)"},
    {"&Redo", "重做(&R)"},
    {"&Select / Move", "选择 / 移动"},
    {"Add &Station", "添加站点(&S)"},
    {"Add &Edge", "添加区间(&E)"},
    {"&Draw Line", "绘制线路(&D)"},
    {"&Delete", "删除(&D)"},
    {"&Finish Line", "结束线路(&F)"},
    {"Remove Last &Stop", "删除最后一个站(&S)"},
    {"Transfer &Times...", "换乘时间(&T)..."},
    {"&Fit Content", "适应窗口(&F)"},
    {"&About", "关于(&A)"},
    {"Main", "主工具栏"},
    {"Ready", "就绪"},

    // 侧边栏
    {"Network", "线网"},
    {"Lines", "线路"},
    {"Route", "导航"},
    {"Edit...", "编辑..."},
    {"Delete", "删除"},
    {"Transfers...", "换乘..."},
    {"From", "起点"},
    {"To", "终点"},
    {"Depart", "出发"},
    {"Find Route", "查询路线"},
    {"Route details appear here", "行程详情将显示在这里"},

    // 状态与提示
    {"Loaded demo network", "已加载示例线网"},
    {"New document", "新建文档"},
    {"Loaded", "已加载"},
    {"Saved", "已保存"},
    {"Undo", "撤销"},
    {"Redo", "重做"},
    {"Line updated", "线路已更新"},
    {"Line deleted", "线路已删除"},
    {"Transfer times updated", "换乘时间已更新"},
    {"Language changed", "语言已切换"},
    {"Open failed", "打开失败"},
    {"Save failed", "保存失败"},
    {"Delete failed", "删除失败"},
    {"Transfer update failed", "换乘时间更新失败"},
    {"Invalid line", "线路参数无效"},
    {"Unsaved changes", "未保存的修改"},
    {"Save changes before continuing?", "继续前是否保存修改？"},
    {"Open map", "打开地图"},
    {"Open line file", "打开线路文件"},
    {"Save map", "保存地图"},
    {"Save line file", "保存线路文件"},
    {"UrbanChain map (*.cuc3m3 *.cuc3m2 *.cuc3m);;All files (*)",
     "UrbanChain 地图 (*.cuc3m3 *.cuc3m2 *.cuc3m);;所有文件 (*)"},
    {"UrbanChain map v3 (*.cuc3m3)", "UrbanChain 地图 v3 (*.cuc3m3)"},
    {"UrbanChain map v2 (*.cuc3m2)", "UrbanChain 地图 v2 (*.cuc3m2)"},
    {"UrbanChain lines (*.cuc3l);;All files (*)", "UrbanChain 线路 (*.cuc3l);;所有文件 (*)"},
    {"About Concept UrbanChain", "关于 Concept UrbanChain"},
    {"Concept UrbanChain v0.3\nA data-structure course project for metro network editing and time-dependent routing.",
     "Concept UrbanChain v0.3\n数据结构课程设计：地铁线网编辑与时间相关导航。"},
    {"No stations available.", "没有可用站点。"},
    {"Selected station no longer exists.", "所选站点已不存在。"},
    {"No route found for %1 at this time.", "该时刻无法到达 %1。"},
    {"Depart %1   Arrive %2   Transfers %3\n\n", "出发 %1   到达 %2   换乘 %3\n\n"},
    {"%1. %2\n   %3 -> %4\n   %5 - %6\n", "%1. %2\n   %3 -> %4\n   %5 - %6\n"},
    {"Untitled", "未命名"},
    {"%1 | %2 stations | %3 lines", "%1 | %2 个站点 | %3 条线路"},
    {"%1 (%2 stops)", "%1（%2 站）"},

    // 模式
    {"Select", "选择"},
    {"Add station", "添加站点"},
    {"Add edge", "添加区间"},
    {"Draw line", "绘制线路"},
    {"Mode: %1", "模式：%1"},

    // 地图交互
    {"Click an empty area to place a station", "请点击空白处放置站点"},
    {"New station", "新建站点"},
    {"Station name:", "站点名称："},
    {"Click the first station", "请点击第一个站点"},
    {"Click the second station", "请点击第二个站点"},
    {"Choose a different station", "请选择不同的站点"},
    {"Edge added", "区间已添加"},
    {"Click a station to add it to the line", "请点击要加入线路的站点"},
    {"Station added to line; double-click or right-click to finish",
     "站点已加入线路；双击或右键结束"},
    {"Nothing to delete here", "此处没有可删除的对象"},
    {"Station added", "站点已添加"},
    {"Rename station", "重命名站点"},
    {"Station renamed", "站点已重命名"},
    {"Station deleted", "站点已删除"},
    {"Line discarded: at least two stations are required", "线路已放弃：至少需要两个站点"},
    {"Line finished", "线路已结束"},
    {"No line is being drawn", "当前没有正在绘制的线路"},
    {"Station moved", "站点已移动"},
    {"Edge creation cancelled", "已取消添加区间"},
    {"Rename Station...", "重命名站点..."},
    {"Delete Station", "删除站点"},
    {"Edit Line...", "编辑线路..."},
    {"Delete Line", "删除线路"},
    {"Edit Edge...", "编辑区间..."},
    {"Delete Edge", "删除区间"},
    {"Add Station Here", "在此添加站点"},
    {"Edge Properties", "区间属性"},
    {"Cost", "成本"},
    {"Length", "长度"},
    {"Edge updated", "区间已更新"},

    // 线路属性
    {"Line Properties", "线路属性"},
    {"Name", "名称"},
    {"Color", "颜色"},
    {"Dwell at stops (s)", "停站时间（秒）"},
    {"Stops (top to bottom)", "站点（从上到下）"},
    {"Remove stop", "删除站点"},
    {"Move up", "上移"},
    {"Move down", "下移"},
    {"Segment run times (s)", "区段运行时间（秒）"},
    {"Run (s)", "运行时间（秒）"},
    {"Headway periods (seconds since midnight)", "发车间隔时段（自零点起的秒数）"},
    {"Start", "开始"},
    {"End", "结束"},
    {"Interval (s)", "间隔（秒）"},
    {"Add period", "添加时段"},
    {"Remove period", "删除时段"},
    {"Line color", "线路颜色"},

    // 换乘
    {"Transfer Times", "换乘时间"},
    {"Transfer walk times for shared stations. Each ordered line pair is listed once.",
     "共享站点的换乘走行时间。每个有向线路对只列一次。"},
    {"Station", "站点"},
    {"From line", "来自线路"},
    {"To line", "换到线路"},
    {"Seconds", "秒数"},

    // 核心错误（固定条目）
    {"invalid station index", "站点索引无效"},
    {"invalid line index", "线路索引无效"},
    {"failed to add station", "添加站点失败"},
    {"failed to move station", "移动站点失败"},
    {"failed to rename station", "重命名站点失败"},
    {"failed to add edge", "添加区间失败"},
    {"failed to remove edge", "删除区间失败"},
    {"failed to update edge", "更新区间失败"},
    {"edge does not exist", "区间不存在"},
    {"cannot connect a station to itself", "不能把站点连接到自身"},
    {"edge already exists", "区间已存在"},
    {"failed to remove line", "删除线路失败"},
    {"line already contains this station", "线路已包含该站点"},
    {"consecutive stations have no map edge", "相邻站点之间没有地图边"},
    {"consecutive line stations have no map edge", "线路相邻站点之间没有地图边"},
    {"line repeats a station", "线路重复经过同一站点"},
    {"line has fewer than 2 stations", "线路少于两个站点"},
    {"line references an invalid station index", "线路引用了无效站点"},
    {"segmentRunSec size does not match station count - 1",
     "区段运行时间数量与站间数不一致"},
    {"segment run time is negative", "区段运行时间为负"},
    {"dwell time is negative", "停站时间为负"},
    {"line has no headway periods", "线路没有发车间隔时段"},
    {"invalid headway period", "发车间隔时段无效"},
    {"headway periods overlap or are not sorted", "发车间隔时段重叠或未排序"},
    {"transfer lines must differ", "换乘的两条线路必须不同"},
    {"transfer time cannot be negative", "换乘时间不能为负"},
    {"failed to set transfer time", "设置换乘时间失败"},
    {"edge cost and length cannot be negative", "区间成本与长度不能为负"},
    {"BACKGROUND expects a layer count", "BACKGROUND 需要要素数量"},
    {"background layers require CUC3M 2", "背景图层需要 CUC3M 2 格式"},
    {"duplicate BACKGROUND line", "重复的 BACKGROUND 行"},
    {"BG appears before BACKGROUND", "BG 出现在 BACKGROUND 之前"},
    {"more BG lines than BACKGROUND count", "BG 行数多于 BACKGROUND 声明数量"},
    {"BG expects kind, closed flag and id", "BG 需要类型、闭合标志和 ID"},
    {"BG expects a non-negative width after id", "BG 的 ID 后需要非负宽度"},
    {"map files must use .cuc3m2 or .cuc3m3 extension",
     "地图文件必须使用 .cuc3m2 或 .cuc3m3 扩展名"},
    {"CUC3M2 cannot store background widths; save as .cuc3m3",
     "CUC3M2 无法保存线状背景宽度；请保存为 .cuc3m3"},
    {"BG needs at least 2 points", "BG 至少需要两个点"},
    {"closed BG needs at least 3 points", "闭合 BG 至少需要三个点"},
    {"BACKGROUND count does not match BG lines", "BACKGROUND 数量与 BG 行数不一致"},
    {"STATION expects id, x, y", "STATION 需要 ID、x、y"},
    {"EDGE expects two station ids and a cost", "EDGE 需要两个站点 ID 和成本"},
    {"EDGE references an unknown station", "EDGE 引用了未知站点"},
    {"invalid or duplicate EDGE", "EDGE 无效或重复"},
    {"expected CUC3M header", "缺少 CUC3M 文件头"},
    {"unsupported CUC3M version", "不支持的 CUC3M 版本"},
    {"expected CUC3L header", "缺少 CUC3L 文件头"},
    {"unsupported CUC3L version", "不支持的 CUC3L 版本"},
    {"map file is empty or has no header", "地图文件为空或缺少文件头"},
    {"map file has no END marker", "地图文件缺少 END 标记"},
    {"line file is empty or has no header", "线路文件为空或缺少文件头"},
    {"line file has no END marker", "线路文件缺少 END 标记"},
    {"background layers are not supported in v3", "v3 暂不支持背景图层"},
};

const char* chineseFor(const char* english) {
    for (const Entry& entry : kEntries) {
        if (std::strcmp(entry.english, english) == 0) {
            return entry.chinese;
        }
    }
    return nullptr;
}

QString translateExact(const QString& english) {
    if (g_language == Language::English) {
        return english;
    }
    const QByteArray utf8 = english.toUtf8();
    const char* chinese = chineseFor(utf8.constData());
    return chinese == nullptr ? english : QString::fromUtf8(chinese);
}

void installQtTranslations(bool chinese) {
    QCoreApplication* app = QCoreApplication::instance();
    if (g_translator != nullptr && app != nullptr) {
        app->removeTranslator(g_translator);
        delete g_translator;
        g_translator = nullptr;
    }
    if (!chinese || app == nullptr) {
        return;
    }

    QStringList directories;
    directories << QCoreApplication::applicationDirPath() + "/translations";
    directories << QCoreApplication::applicationDirPath() + "/../translations";
    directories << QLibraryInfo::path(QLibraryInfo::TranslationsPath);

    for (const QString& directory : directories) {
        if (!QFileInfo::exists(directory + "/qtbase_zh_CN.qm")) {
            continue;
        }
        auto* translator = new QTranslator;
        if (translator->load("qtbase_zh_CN", directory)) {
            app->installTranslator(translator);
            g_translator = translator;
            return;
        }
        delete translator;
    }
}

QString prefixLocalize(const QString& message) {
    const QString linePrefix = "line ";
    if (message.startsWith(linePrefix)) {
        const int colon = message.indexOf(": ", linePrefix.size());
        if (colon > static_cast<int>(linePrefix.size())) {
            const QString number = message.mid(linePrefix.size(),
                                               colon - linePrefix.size());
            bool ok = false;
            number.toInt(&ok);
            if (ok) {
                return QString("第 %1 行：").arg(number) +
                       localizeMessage(message.mid(colon + 2));
            }
        }
    }

    const QString stationPrefix = "station is used by line ";
    const QString stationSuffix = "; remove it from the line first";
    if (message.startsWith(stationPrefix) && message.endsWith(stationSuffix)) {
        const int length = message.size() - stationPrefix.size() - stationSuffix.size();
        return QString("该站点仍被线路 %1 使用；请先从该线路中移除")
            .arg(message.mid(stationPrefix.size(), length));
    }

    const QString edgePrefix = "edge is used by line ";
    const QString edgeSuffix = "; remove the line first";
    if (message.startsWith(edgePrefix) && message.endsWith(edgeSuffix)) {
        const int length = message.size() - edgePrefix.size() - edgeSuffix.size();
        return QString("该区间仍被线路 %1 使用；请先删除该线路")
            .arg(message.mid(edgePrefix.size(), length));
    }

    const QString deletePrefix = "cannot delete station: ";
    if (message.startsWith(deletePrefix)) {
        return QString("无法删除站点：") +
               localizeMessage(message.mid(deletePrefix.size()));
    }
    const QString savePrefix = "cannot save: ";
    if (message.startsWith(savePrefix)) {
        return QString("无法保存：") + localizeMessage(message.mid(savePrefix.size()));
    }
    const QString unknownStation = "unknown station: ";
    if (message.startsWith(unknownStation)) {
        return QString("未知站点：") + message.mid(unknownStation.size());
    }
    const QString unknownKeyword = "unknown keyword: ";
    if (message.startsWith(unknownKeyword)) {
        return QString("未知关键字：") + message.mid(unknownKeyword.size());
    }
    const QString unknownBackground = "unknown background kind: ";
    if (message.startsWith(unknownBackground)) {
        return QString("未知背景类型：") + message.mid(unknownBackground.size());
    }
    const QString invalidColor = "invalid line color: ";
    if (message.startsWith(invalidColor)) {
        return QString("线路颜色无效：") + message.mid(invalidColor.size());
    }
    const QString duplicateStation = "duplicate station id: ";
    if (message.startsWith(duplicateStation)) {
        return QString("重复的站点 ID：") + message.mid(duplicateStation.size());
    }
    const QString openMap = "cannot open map file: ";
    if (message.startsWith(openMap)) {
        return QString("无法打开地图文件：") + message.mid(openMap.size());
    }
    const QString openLines = "cannot open line file: ";
    if (message.startsWith(openLines)) {
        return QString("无法打开线路文件：") + message.mid(openLines.size());
    }
    const QString writeMap = "failed to write map file: ";
    if (message.startsWith(writeMap)) {
        return QString("写入地图文件失败：") + message.mid(writeMap.size());
    }
    const QString writeLines = "failed to write line file: ";
    if (message.startsWith(writeLines)) {
        return QString("写入线路文件失败：") + message.mid(writeLines.size());
    }
    const QString writeMapOpen = "cannot open map file for writing: ";
    if (message.startsWith(writeMapOpen)) {
        return QString("无法写入地图文件：") + message.mid(writeMapOpen.size());
    }
    const QString writeLinesOpen = "cannot open line file for writing: ";
    if (message.startsWith(writeLinesOpen)) {
        return QString("无法写入线路文件：") + message.mid(writeLinesOpen.size());
    }
    return message;
}

}  // namespace

Language currentLanguage() { return g_language; }

void setLanguage(Language language, bool persist) {
    g_language = language;
    if (persist) {
        QSettings settings;
        settings.setValue("ui/language",
                          language == Language::Chinese ? QString("zh") : QString("en"));
    }
    installQtTranslations(language == Language::Chinese);
}

void initializeLanguage() {
    QSettings settings;
    const QString stored = settings.value("ui/language").toString();
    if (stored == "zh") {
        g_language = Language::Chinese;
    } else if (stored == "en") {
        g_language = Language::English;
    } else {
        g_language = QLocale::system().language() == QLocale::Chinese
                         ? Language::Chinese
                         : Language::English;
    }
    installQtTranslations(g_language == Language::Chinese);
}

QString L(const char* english) { return translateExact(QString::fromUtf8(english)); }

QString localizeMessage(const QString& english) {
    if (g_language == Language::English) {
        return english;
    }
    const QString exact = translateExact(english);
    if (exact != english) {
        return exact;
    }
    return prefixLocalize(english);
}

}  // namespace uc
