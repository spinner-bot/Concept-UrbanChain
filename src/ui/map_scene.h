#pragma once

#include <QGraphicsScene>
#include <QPointF>

#include "ds/dynamic_array.h"

namespace uc {

class Editor;
class StationItem;

class MapScene : public QGraphicsScene {
public:
    explicit MapScene(Editor& editor, QObject* parent = nullptr);

    void refresh();

    void setSelectedStation(int stationIndex);
    int selectedStation() const { return selectedStation_; }

    void setPreview(const QPointF& scenePos, int pendingStation, int activeLine);
    void clearPreview();

    StationItem* stationItem(int stationIndex) const;
    void setStationVisualPosition(int stationIndex, const QPointF& position);

    // 返回距离 scenePos 最近且在 threshold 内的线路索引，未命中返回 -1。
    int lineAt(const QPointF& scenePos, double threshold) const;

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    Editor& editor_;
    DynamicArray<StationItem*> stationItems_;
    int selectedStation_ = -1;
    QPointF previewPos_;
    bool hasPreview_ = false;
    int pendingStation_ = -1;
    int activeLine_ = -1;
};

}  // namespace uc
