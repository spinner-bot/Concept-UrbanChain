#include "ui/map_scene.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

#include "core/network.h"
#include "core/spline.h"
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
    bool hasBounds = false;

    auto extendBounds = [&](double x, double y) {
        if (!hasBounds) {
            minX = maxX = x;
            minY = maxY = y;
            hasBounds = true;
        } else {
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    };

    for (Index i = 0; i < map.stationCount(); ++i) {
        const Station& station = map.station(i);
        const double sceneY = -station.position.y;
        auto* item = new StationItem(i, QString::fromUtf8(station.name.c_str()));
        item->setPos(station.position.x, sceneY);
        addItem(item);
        stationItems_.push_back(item);
        extendBounds(station.position.x, sceneY);
    }
    for (std::size_t i = 0; i < map.backgroundFeatures().size(); ++i) {
        const BackgroundFeature& feature = map.backgroundFeatures()[i];
        for (std::size_t j = 0; j < feature.points.size(); ++j) {
            extendBounds(feature.points[j].x, -feature.points[j].y);
        }
    }

    if (hasBounds) {
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

DynamicArray<Vec2> MapScene::lineSamples(const Line& line) const {
    const Network& network = editor_.network();
    DynamicArray<Vec2> keys;
    for (std::size_t i = 0; i < line.stations().size(); ++i) {
        const QPointF point = displayPoint(network, line.stations()[i]);
        keys.push_back(Vec2{point.x(), point.y()});
    }
    return catmullRomSpline(keys, 24, 0.5);
}

QPainterPath MapScene::buildLinePath(const Line& line, const QPointF* extra) const {
    const Network& network = editor_.network();
    DynamicArray<Vec2> keys;
    for (std::size_t i = 0; i < line.stations().size(); ++i) {
        const QPointF point = displayPoint(network, line.stations()[i]);
        keys.push_back(Vec2{point.x(), point.y()});
    }
    if (extra != nullptr) {
        keys.push_back(Vec2{extra->x(), extra->y()});
    }
    const DynamicArray<Vec2> samples = catmullRomSpline(keys, 24, 0.5);
    QPainterPath path;
    if (!samples.empty()) {
        path.moveTo(samples[0].x, samples[0].y);
        for (std::size_t i = 1; i < samples.size(); ++i) {
            path.lineTo(samples[i].x, samples[i].y);
        }
    }
    return path;
}

void MapScene::paintBackgroundFeatures(QPainter* painter, const Map& map) const {
    const DynamicArray<BackgroundFeature>& features = map.backgroundFeatures();
    if (features.empty()) {
        return;
    }

    auto scenePoints = [](const BackgroundFeature& feature) {
        DynamicArray<Vec2> points;
        for (std::size_t i = 0; i < feature.points.size(); ++i) {
            points.push_back(Vec2{feature.points[i].x, -feature.points[i].y});
        }
        return points;
    };
    auto makePath = [](const DynamicArray<Vec2>& points, bool closed) {
        QPainterPath path;
        if (!points.empty()) {
            path.moveTo(points[0].x, points[0].y);
            for (std::size_t i = 1; i < points.size(); ++i) {
                path.lineTo(points[i].x, points[i].y);
            }
        }
        if (closed) {
            path.closeSubpath();
        }
        return path;
    };
    auto strokeBand = [painter, &makePath](const DynamicArray<Vec2>& points, double width,
                                           const QColor& color, bool smooth) {
        if (points.size() < 2) {
            return;
        }
        const DynamicArray<Vec2> samples =
            smooth ? catmullRomSpline(points, 16, 0.5) : points;
        QPen pen(color);
        pen.setCosmetic(false);
        pen.setWidthF(std::max(width, 0.001));
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(makePath(samples, false));
    };

    // 1. 面状底图：城市区域、公园、湖泊与闭合水体。
    for (std::size_t i = 0; i < features.size(); ++i) {
        const BackgroundFeature& feature = features[i];
        if (!feature.closed || feature.points.size() < 3) {
            continue;
        }
        QColor fill;
        QColor stroke;
        if (feature.kind == BackgroundKind::Region) {
            fill = QColor("#f3efe6");
            stroke = QColor("#d8cdbb");
        } else if (feature.kind == BackgroundKind::Park) {
            fill = QColor("#cfe6c4");
            stroke = QColor("#a6c99a");
        } else if (feature.kind == BackgroundKind::Water ||
                   feature.kind == BackgroundKind::River) {
            fill = QColor("#b7d9e8");
            stroke = QColor("#8fbcd0");
        } else {
            continue;
        }
        const DynamicArray<Vec2> points = scenePoints(feature);
        QPen pen(stroke);
        pen.setCosmetic(true);
        pen.setWidthF(1.2);
        painter->setPen(pen);
        painter->setBrush(fill);
        painter->drawPath(makePath(points, true));
    }
    painter->setBrush(Qt::NoBrush);

    // 道路、河流等底图线状要素裁剪到城市区域，避免伸出陆地边界。
    QPainterPath regionClip;
    for (std::size_t i = 0; i < features.size(); ++i) {
        const BackgroundFeature& feature = features[i];
        if (feature.kind == BackgroundKind::Region && feature.closed &&
            feature.points.size() >= 3) {
            regionClip.addPath(makePath(scenePoints(feature), true));
        }
    }
    painter->save();
    if (!regionClip.isEmpty()) {
        painter->setClipPath(regionClip);
    }

    // 2. 道路：深色外框 + 浅色路面，宽度按数据坐标缩放。
    for (std::size_t i = 0; i < features.size(); ++i) {
        const BackgroundFeature& feature = features[i];
        if (feature.kind != BackgroundKind::Road || feature.points.size() < 2) {
            continue;
        }
        const DynamicArray<Vec2> points = scenePoints(feature);
        const double width = feature.width > 0.0 ? feature.width : 0.05;
        strokeBand(points, width + std::max(0.012, width * 0.18),
                   QColor("#d8d0c0"), false);
        strokeBand(points, width * 0.72, QColor("#fffdf7"), false);
    }

    // 3. 河流：河岸 + 水面，宽度来自数据。
    for (std::size_t i = 0; i < features.size(); ++i) {
        const BackgroundFeature& feature = features[i];
        if (feature.kind != BackgroundKind::River || feature.closed ||
            feature.points.size() < 2) {
            continue;
        }
        const DynamicArray<Vec2> points = scenePoints(feature);
        const double width = feature.width > 0.0 ? feature.width : 0.16;
        strokeBand(points, width + std::max(0.02, width * 0.18),
                   QColor("#7faec6"), true);
        strokeBand(points, width, QColor("#a9d3e6"), true);
    }

    // 4. 海岸线。
    for (std::size_t i = 0; i < features.size(); ++i) {
        const BackgroundFeature& feature = features[i];
        if (feature.kind != BackgroundKind::Coast || feature.points.size() < 2) {
            continue;
        }
        const DynamicArray<Vec2> points = scenePoints(feature);
        const double width = feature.width > 0.0 ? feature.width : 0.03;
        strokeBand(points, width, QColor("#4f86a6"), true);
    }
    painter->restore();

    // 5. 行政边界、山体/等高线以及未闭合的其他线状要素。
    for (std::size_t i = 0; i < features.size(); ++i) {
        const BackgroundFeature& feature = features[i];
        if (feature.points.size() < 2) {
            continue;
        }
        if (feature.kind == BackgroundKind::Road ||
            feature.kind == BackgroundKind::River ||
            feature.kind == BackgroundKind::Coast ||
            (feature.closed &&
             (feature.kind == BackgroundKind::Region ||
              feature.kind == BackgroundKind::Park ||
              feature.kind == BackgroundKind::Water))) {
            continue;
        }
        const DynamicArray<Vec2> points = scenePoints(feature);
        QPen pen(QColor("#b3a68d"));
        pen.setCosmetic(true);
        pen.setWidthF(1.1);
        pen.setStyle(Qt::DashLine);
        if (feature.kind == BackgroundKind::Mountain) {
            pen.setColor(QColor("#b3a48c"));
            pen.setWidthF(1.0);
        }
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(makePath(points, feature.closed));
    }
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
        const DynamicArray<Vec2> samples = lineSamples(line);
        for (std::size_t i = 0; i + 1 < samples.size(); ++i) {
            const QPointF a(samples[i].x, samples[i].y);
            const QPointF b(samples[i + 1].x, samples[i + 1].y);
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

    paintBackgroundFeatures(painter, map);

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
        const QPainterPath path = buildLinePath(line);
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

        const QPainterPath path = buildLinePath(line, &previewPos_);
        painter->drawPath(path);
    }
}

}  // namespace uc
