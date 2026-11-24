#pragma once

#include <cstdint>
#include <string>

#include "core/network.h"
#include "ds/dynamic_array.h"

namespace uc {

// 与界面无关的编辑控制器。
// 图形界面只负责把鼠标/菜单事件翻译成这里的调用，并读取 network() 渲染；
// 后续替换为 Web 前端时，这一层可以原样复用。
class Editor {
public:
    Editor() = default;

    const Network& network() const { return network_; }

    void newDocument();
    bool load(const std::string& mapPath, const std::string& linePath, std::string* error);
    bool save(const std::string& mapPath, const std::string& linePath, std::string* error);

    bool isDirty() const { return dirty_; }
    bool validate(std::string* error) const;

    // 站点
    std::string nextStationId() const;
    Index addStation(const std::string& name, double x, double y, std::string* error);
    bool moveStation(Index station, double x, double y, std::string* error);
    bool renameStation(Index station, const std::string& name, std::string* error);
    bool removeStation(Index station, std::string* error);

    // 区间
    bool addEdge(Index from, Index to, double cost, double length, std::string* error);
    bool addEdge(Index from, Index to, std::string* error);
    bool removeEdge(Index from, Index to, std::string* error);

    // 线路
    int addLine(const std::string& name, std::uint32_t color, std::string* error);
    bool removeLine(int line, std::string* error);
    bool appendStop(int line, Index station, std::string* error);
    bool removeLastStop(int line, std::string* error);
    bool setLineStations(int line, const DynamicArray<Index>& stations, std::string* error);
    bool replaceLine(int line, Line updated, std::string* error);

    bool setTransferTime(Index station, int fromLine, int toLine, double seconds,
                         std::string* error);

    // 撤销 / 重做
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    bool undo();
    bool redo();

    static constexpr double kDefaultSegmentRunSec = 120.0;

private:
    bool checkStation(Index station, std::string* error) const;
    bool checkLine(int line, std::string* error) const;
    void pushUndo();
    void rollbackUndo();

    Network network_;
    DynamicArray<Network> undo_;
    DynamicArray<Network> redo_;
    bool dirty_ = false;
};

}  // namespace uc
