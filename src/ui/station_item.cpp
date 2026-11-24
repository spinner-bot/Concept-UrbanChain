#include "ui/station_item.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QGraphicsSimpleTextItem>

namespace uc {

StationItem::StationItem(int stationIndex, const QString& name, QGraphicsItem* parent)
    : QGraphicsEllipseItem(-7.0, -7.0, 14.0, 14.0, parent), stationIndex_(stationIndex) {
    setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    setZValue(10.0);
    setToolTip(name);

    normalPen_ = QPen(QColor("#1f2937"), 1.5);
    selectedPen_ = QPen(QColor("#b45309"), 2.5);
    setPen(normalPen_);
    setBrush(QBrush(QColor("#ffffff")));

    auto* label = new QGraphicsSimpleTextItem(name, this);
    label->setBrush(QColor("#111827"));
    QFont font = label->font();
    font.setPointSize(9);
    label->setFont(font);
    label->setPos(9.0, -20.0);
}

void StationItem::setSelectedVisual(bool selected) {
    setPen(selected ? selectedPen_ : normalPen_);
    setBrush(QBrush(selected ? QColor("#fde68a") : QColor("#ffffff")));
}

}  // namespace uc
