#pragma once

#include <QGraphicsEllipseItem>
#include <QPen>

class QGraphicsSimpleTextItem;

namespace uc {

// 站点图元：位置使用场景坐标，大小固定为屏幕像素，缩放时保持可点击。
class StationItem : public QGraphicsEllipseItem {
public:
    StationItem(int stationIndex, const QString& name, QGraphicsItem* parent = nullptr);

    int stationIndex() const { return stationIndex_; }
    void setSelectedVisual(bool selected);

private:
    int stationIndex_ = -1;
    QPen normalPen_;
    QPen selectedPen_;
};

}  // namespace uc
