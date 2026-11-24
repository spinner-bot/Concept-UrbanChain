#include "ui/map_scene.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

#include "core/network.h"
#include "edit/editor.h"
#include "ui/station_item.h"

namespace uc {
namespace {

QColor lineColor(std::uint32_t rgb) {
    return QColor::fromRgb(static_cast<QRgb>(rgb | 0xff000000u));
}

double distanceToSegment(const QPointF& point, const QPointF& a, const QPointF& b) {
    const QPointF ab = b - a;
    const double lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();
    if (lengthSquared <= 1e-12) {
        return std::hypot(point.x() - a.x(), point.y() - a.y());
    }
    double t = ((point.x() - a.x()) * ab.x() + (point.y() - a.y()) * ab.y()) / lengthSquared;
    if (t < 0.0) {
        t = 0.0;
    } else if (t > 1.0) {
        t = 1.0;
    }
    const QPointF projection = a + t * ab;
    return std::hypot(point.x() - projection.x(), point.y() - projection.y());
}

QPointF stationPoint(const Network& network, Index station) {
    const Vec2& position = network.map().station(station).position;
    // 地图坐标 y 轴向上，Qt 场景坐标 y 轴向下。
    return QPointF(position.x, -position.y);
}

}  // namespace

MapScene::MapScene(Editor& editor, QObject* parent) : QGraphicsScene(parent), editor_(editor) {}

void MapScene::refresh() {
    clear();
    stationItems_.clear();

    const Map& map = editor_.network().map();
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    bool hasStation = false;

    for (Index i = 0; i < map.stationCount(); ++i) {
        const Station& station = map.station(i);
        const double sceneY = -station.position.y;
        auto* item = new StationItem(i, QString::fromUtf8(station.name.c_str()));
        item->setPos(station.position.x, sceneY);
        addItem(item);
        stationItems_.push_back(item);

        if (!hasStation) {
            minX = maxX = station.position.x;
            minY = maxY = sceneY;
            hasStation = true;
        } else {
            minX = std::min(minX, station.position.x);
            maxX = std::max(maxX, station.position.x);
            minY = std::min(minY, sceneY);
            maxY = std::max(maxY, sceneY);
        }
    }

    if (hasStation) {
        const double span = std::max(maxX - minX, maxY - minY);
        const double margin = std::max(0.5, span * 0.12);
        setSceneRect(QRectF(minX - margin, minY - margin,
                            (maxX - minX) + margin * 2.0, (maxY - minY) + margin * 2.0));
    } else {
        setSceneRect(QRectF(-5.0, -5.0, 10.0, 10.0));
    }

    if (selectedStation_ >= 0) {
        StationItem* item = stationItem(selectedStation_);
        if (item != nullptr) {
            item->setSelectedVisual(true);
        } else {
            selectedStation_ = -1;
        }
    }
    if (selectedLine_ < 0 ||
        selectedLine_ >= static_cast<int>(editor_.network().lines().size())) {
        selectedLine_ = -1;
    }
    update();
}

void MapScene::setSelectedStation(int stationIndex) {
    if (selectedStation_ == stationIndex) {
        return;
    }
    if (StationItem* previous = stationItem(selectedStation_)) {
        previous->setSelectedVisual(false);
    }
    selectedStation_ = stationIndex;
    if (StationItem* current = stationItem(selectedStation_)) {
        current->setSelectedVisual(true);
    }
    update();
}

void MapScene::setSelectedLine(int lineIndex) {
    if (selectedLine_ == lineIndex) {
        return;
    }
    selectedLine_ = lineIndex;
    update();
}

void MapScene::setPreview(const QPointF& scenePos, int pendingStation, int activeLine) {
    previewPos_ = scenePos;
    pendingStation_ = pendingStation;
    activeLine_ = activeLine;
    hasPreview_ = true;
    update();
}

void MapScene::clearPreview() {
    hasPreview_ = false;
    pendingStation_ = -1;
    activeLine_ = -1;
    update();
}

StationItem* MapScene::stationItem(int stationIndex) const {
    if (stationIndex < 0) {
        return nullptr;
    }
    for (std::size_t i = 0; i < stationItems_.size(); ++i) {
        if (stationItems_[i]->stationIndex() == stationIndex) {
            return stationItems_[i];
        }
    }
    return nullptr;
}

void MapScene::setStationVisualPosition(int stationIndex, const QPointF& position) {
    if (StationItem* item = stationItem(stationIndex)) {
        item->setPos(position);
        update();
    }
}

void MapScene::setDraggingStation(int stationIndex, const QPointF& scenePosition) {
    draggedStation_ = stationIndex;
    draggedPosition_ = scenePosition;
    update();
}

void MapScene::clearDraggingStation() {
    draggedStation_ = -1;
    update();
}

QPointF MapScene::displayPoint(const Network& network, Index station) const {
    if (station == draggedStation_) {
        return draggedPosition_;
    }
    return stationPoint(network, station);
}

int MapScene::edgeAt(const QPointF& scenePos, double threshold) const {
    const Network& network = editor_.network();
    const Map& map = network.map();
    int bestEdge = -1;
    double bestDistance = threshold;
    for (std::size_t i = 0; i < map.edges().size(); ++i) {
        const EdgeRecord& edge = map.edges()[i];
        const QPointF a = stationPoint(network, edge.from);
        const QPointF b = stationPoint(network, edge.to);
        const double distance = distanceToSegment(scenePos, a, b);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestEdge = static_cast<int>(i);
        }
    }
    return bestEdge;
}

int MapScene::lineAt(const QPointF& scenePos, double threshold) const {
    const Network& network = editor_.network();
    int bestLine = -1;
    double bestDistance = threshold;
    for (std::size_t lineIndex = 0; lineIndex < network.lines().size(); ++lineIndex) {
        const Line& line = network.lines()[lineIndex];
        if (line.stationCount() < 2) {
            continue;
        }
        for (int i = 0; i + 1 < line.stationCount(); ++i) {
            const QPointF a = stationPoint(network, line.stations()[static_cast<std::size_t>(i)]);
            const QPointF b =
                stationPoint(network, line.stations()[static_cast<std::size_t>(i + 1)]);
            const double distance = distanceToSegment(scenePos, a, b);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestLine = static_cast<int>(lineIndex);
            }
        }
    }
    return bestLine;
}

void MapScene::drawBackground(QPainter* painter, const QRectF& rect) {
    paintNetwork(painter, rect);
}

void MapScene::paintNetwork(QPainter* painter, const QRectF& rect) const {
    Q_UNUSED(rect);
    const Network& network = editor_.network();
    const Map& map = network.map();

    QPen edgePen(QColor("#9ca3af"));
    edgePen.setCosmetic(true);
    edgePen.setWidthF(2.0);
    painter->setPen(edgePen);
    for (std::size_t i = 0; i < map.edges().size(); ++i) {
        const EdgeRecord& edge = map.edges()[i];
        painter->drawLine(displayPoint(network, edge.from), displayPoint(network, edge.to));
    }

    for (std::size_t lineIndex = 0; lineIndex < network.lines().size(); ++lineIndex) {
        const Line& line = network.lines()[lineIndex];
        if (line.stationCount() < 2) {
            continue;
        }
        QPainterPath path;
        path.moveTo(displayPoint(network, line.stations()[0]));
        for (int i = 1; i < line.stationCount(); ++i) {
            path.lineTo(displayPoint(network, line.stations()[static_cast<std::size_t>(i)]));
        }
        const QColor color = lineColor(line.color());
        if (static_cast<int>(lineIndex) == selectedLine_) {
            QColor halo = color;
            halo.setAlpha(90);
            QPen haloPen(halo);
            haloPen.setCosmetic(true);
            haloPen.setWidthF(13.0);
            haloPen.setCapStyle(Qt::RoundCap);
            haloPen.setJoinStyle(Qt::RoundJoin);
            painter->setPen(haloPen);
            painter->drawPath(path);
        }
        QPen pen(color);
        pen.setCosmetic(true);
        pen.setWidthF(static_cast<int>(lineIndex) == selectedLine_ ? 6.5 : 5.0);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        painter->drawPath(path);
    }
}

void MapScene::drawForeground(QPainter* painter, const QRectF& rect) {
    paintOverlay(painter, rect);
}

void MapScene::paintOverlay(QPainter* painter, const QRectF& rect) const {
    Q_UNUSED(rect);
    const Network& network = editor_.network();

    if (hasPreview_ && pendingStation_ >= 0) {
        QPen pen(QColor("#2563eb"));
        pen.setCosmetic(true);
        pen.setWidthF(2.0);
        pen.setStyle(Qt::DashLine);
        painter->setPen(pen);
        painter->drawLine(stationPoint(network, pendingStation_), previewPos_);
    }

    if (hasPreview_ && activeLine_ >= 0 &&
        activeLine_ < static_cast<int>(network.lines().size())) {
        const Line& line = network.lines()[static_cast<std::size_t>(activeLine_)];
        QColor color = lineColor(line.color());
        color.setAlpha(180);
        QPen pen(color);
        pen.setCosmetic(true);
        pen.setWidthF(4.0);
        pen.setStyle(Qt::DashLine);
        pen.setCapStyle(Qt::RoundCap);
        painter->setPen(pen);

        QPainterPath path;
        if (line.stationCount() > 0) {
            path.moveTo(stationPoint(network, line.stations()[0]));
            for (int i = 1; i < line.stationCount(); ++i) {
                path.lineTo(stationPoint(network, line.stations()[static_cast<std::size_t>(i)]));
            }
            path.lineTo(previewPos_);
        }
        painter->drawPath(path);
    }
}

}  // namespace uc
