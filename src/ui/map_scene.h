#pragma once

#include <QGraphicsScene>
#include <QPainterPath>
#include <QPointF>

#include "core/map.h"
#include "core/types.h"
#include "ds/dynamic_array.h"

namespace uc {

class Editor;
class Line;
class Network;
class StationItem;

class MapScene : public QGraphicsScene {
public:
    explicit MapScene(Editor& editor, QObject* parent = nullptr);

    void refresh();

    void setSelectedStation(int stationIndex);
    int selectedStation() const { return selectedStation_; }

    void setSelectedLine(int lineIndex);
    int selectedLine() const { return selectedLine_; }

    void setPreview(const QPointF& scenePos, int pendingStation, int activeLine);
    void clearPreview();

    StationItem* stationItem(int stationIndex) const;
    void setStationVisualPosition(int stationIndex, const QPointF& position);
    void setDraggingStation(int stationIndex, const QPointF& scenePosition);
    void clearDraggingStation();

    // 返回距离 scenePos 最近且在 threshold 内的线路索引，未命中返回 -1。
    int lineAt(const QPointF& scenePos, double threshold) const;
    // 返回距离 scenePos 最近且在 threshold 内的区间索引，未命中返回 -1。
    int edgeAt(const QPointF& scenePos, double threshold) const;

    // 供 QGraphicsView 的绘制回调使用。
    void paintNetwork(QPainter* painter, const QRectF& rect) const;
    void paintOverlay(QPainter* painter, const QRectF& rect) const;

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    QPointF displayPoint(const Network& network, Index station) const;
    DynamicArray<Vec2> lineSamples(const Line& line) const;
    QPainterPath buildLinePath(const Line& line, const QPointF* extra = nullptr) const;
    void paintBackgroundFeatures(QPainter* painter, const Map& map) const;

    Editor& editor_;
    DynamicArray<StationItem*> stationItems_;
    int selectedStation_ = -1;
    int selectedLine_ = -1;
    int draggedStation_ = -1;
    QPointF draggedPosition_;
    QPointF previewPos_;
    bool hasPreview_ = false;
    int pendingStation_ = -1;
    int activeLine_ = -1;
};

}  // namespace uc
