#include "ui/map_view.h"

#include <QColor>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/network.h"
#include "edit/editor.h"
#include "ui/map_scene.h"
#include "ui/station_item.h"

namespace uc {
namespace {

const char* modeName(EditMode mode) {
    switch (mode) {
        case EditMode::Select:
            return "Select";
        case EditMode::AddStation:
            return "Add station";
        case EditMode::AddEdge:
            return "Add edge";
        case EditMode::DrawLine:
            return "Draw line";
        case EditMode::DeleteLine:
            return "Delete line";
    }
    return "Unknown";
}

QString fromUtf8(const std::string& text) { return QString::fromUtf8(text.c_str()); }

}  // namespace

MapView::MapView(Editor& editor, QWidget* parent) : QGraphicsView(parent), editor_(editor) {
    scene_ = new MapScene(editor_, this);
    setScene(scene_);
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::TextAntialiasing, true);
    setMouseTracking(true);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::NoDrag);
    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(QColor("#f6f3ea"));
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setCursor(Qt::ArrowCursor);
}

void MapView::setStatusCallback(std::function<void(const QString&)> callback) {
    statusCallback_ = static_cast<std::function<void(const QString&)>&&>(callback);
}

void MapView::setChangeCallback(std::function<void()> callback) {
    changeCallback_ = static_cast<std::function<void()>&&>(callback);
}

void MapView::notifyStatus(const QString& message) {
    if (statusCallback_) {
        statusCallback_(message);
    }
}

void MapView::notifyChanged() {
    if (changeCallback_) {
        changeCallback_();
    }
}

void MapView::refresh() {
    scene_->refresh();
    scene_->setPreview(previewPos_, pendingStation_, activeLine_);
    update();
}

void MapView::setMode(EditMode mode) {
    if (mode_ == mode) {
        return;
    }
    if (mode_ == EditMode::DrawLine && activeLine_ >= 0) {
        finishLine();
    }
    if (mode != EditMode::AddEdge) {
        pendingStation_ = -1;
        scene_->clearPreview();
    }
    mode_ = mode;
    switch (mode_) {
        case EditMode::Select:
            setCursor(Qt::ArrowCursor);
            break;
        case EditMode::AddStation:
        case EditMode::DrawLine:
            setCursor(Qt::CrossCursor);
            break;
        case EditMode::AddEdge:
            setCursor(Qt::PointingHandCursor);
            break;
        case EditMode::DeleteLine:
            setCursor(Qt::CrossCursor);
            break;
    }
    notifyStatus(QString("Mode: ") + modeName(mode_));
}

void MapView::fitContent() {
    if (scene_->sceneRect().isEmpty()) {
        resetTransform();
        return;
    }
    fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
}

void MapView::wheelEvent(QWheelEvent* event) {
    const double factor = std::pow(1.0015, static_cast<double>(event->angleDelta().y()));
    scale(factor, factor);
    event->accept();
}

StationItem* MapView::stationAt(const QPoint& viewPosition) const {
    QGraphicsItem* item = itemAt(viewPosition);
    while (item != nullptr) {
        if (auto* station = dynamic_cast<StationItem*>(item)) {
            return station;
        }
        item = item->parentItem();
    }
    return nullptr;
}

void MapView::handleLeftPress(const QPoint& viewPosition) {
    const QPointF scenePos = mapToScene(viewPosition);
    StationItem* station = stationAt(viewPosition);
    std::string error;

    switch (mode_) {
        case EditMode::Select: {
            if (station != nullptr) {
                const int index = station->stationIndex();
                scene_->setSelectedStation(index);
                dragging_ = true;
                dragMoved_ = false;
                dragStation_ = index;
                dragStartScene_ = scenePos;
                dragStationStart_ = station->pos();
            } else {
                scene_->setSelectedStation(-1);
            }
            break;
        }
        case EditMode::AddStation: {
            if (station != nullptr) {
                notifyStatus("Click an empty area to place a station");
                break;
            }
            bool accepted = false;
            const QString name = QInputDialog::getText(this, "New station", "Station name:",
                                                       QLineEdit::Normal, QString(), &accepted);
            if (!accepted) {
                break;
            }
            const Index index = editor_.addStation(name.toUtf8().constData(), scenePos.x(),
                                                   scenePos.y(), &error);
            if (index == kInvalidIndex) {
                notifyStatus(fromUtf8(error));
                break;
            }
            refresh();
            scene_->setSelectedStation(index);
            notifyStatus("Station added");
            notifyChanged();
            break;
        }
        case EditMode::AddEdge: {
            if (station == nullptr) {
                pendingStation_ = -1;
                scene_->clearPreview();
                notifyStatus("Click the first station");
                break;
            }
            if (pendingStation_ < 0) {
                pendingStation_ = station->stationIndex();
                scene_->setPreview(scenePos, pendingStation_, -1);
                notifyStatus("Click the second station");
                break;
            }
            const int first = pendingStation_;
            const int second = station->stationIndex();
            pendingStation_ = -1;
            if (first == second) {
                scene_->clearPreview();
                notifyStatus("Choose a different station");
                break;
            }
            if (!editor_.addEdge(first, second, &error)) {
                notifyStatus(fromUtf8(error));
                break;
            }
            refresh();
            notifyStatus("Edge added");
            notifyChanged();
            break;
        }
        case EditMode::DrawLine: {
            if (station == nullptr) {
                notifyStatus("Click a station to add it to the line");
                break;
            }
            if (activeLine_ < 0) {
                activeLine_ = editor_.addLine("", nextLineColor(), &error);
                if (activeLine_ < 0) {
                    notifyStatus(fromUtf8(error));
                    break;
                }
            }
            if (!editor_.appendStop(activeLine_, station->stationIndex(), &error)) {
                notifyStatus(fromUtf8(error));
                break;
            }
            refresh();
            notifyStatus("Station added to line; double-click or right-click to finish");
            notifyChanged();
            break;
        }
        case EditMode::DeleteLine: {
            const double scale = std::max(0.0001, transform().m11());
            const int line = scene_->lineAt(scenePos, 10.0 / scale);
            if (line < 0) {
                notifyStatus("Click closer to a line");
                break;
            }
            if (!editor_.removeLine(line, &error)) {
                notifyStatus(fromUtf8(error));
                break;
            }
            refresh();
            notifyStatus("Line deleted");
            notifyChanged();
            break;
        }
    }
}

void MapView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        lastPan_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton) {
        if (activeLine_ >= 0) {
            finishLine();
        } else {
            pendingStation_ = -1;
            scene_->clearPreview();
        }
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        handleLeftPress(event->pos());
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void MapView::mouseMoveEvent(QMouseEvent* event) {
    if (panning_) {
        const QPoint delta = event->pos() - lastPan_;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        lastPan_ = event->pos();
        event->accept();
        return;
    }
    if (dragging_) {
        const QPointF scenePos = mapToScene(event->pos());
        const QPointF delta = scenePos - dragStartScene_;
        if (std::abs(delta.x()) > 1e-6 || std::abs(delta.y()) > 1e-6) {
            dragMoved_ = true;
        }
        scene_->setStationVisualPosition(dragStation_, dragStationStart_ + delta);
        event->accept();
        return;
    }
    previewPos_ = mapToScene(event->pos());
    scene_->setPreview(previewPos_, pendingStation_, activeLine_);
    event->accept();
}

void MapView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && panning_) {
        panning_ = false;
        setCursor(mode_ == EditMode::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        if (dragMoved_) {
            const QPointF scenePos = mapToScene(event->pos());
            const QPointF delta = scenePos - dragStartScene_;
            const QPointF target = dragStationStart_ + delta;
            std::string error;
            if (!editor_.moveStation(dragStation_, target.x(), target.y(), &error)) {
                notifyStatus(fromUtf8(error));
            } else {
                notifyStatus("Station moved");
                notifyChanged();
            }
            refresh();
        }
        dragStation_ = -1;
        dragMoved_ = false;
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void MapView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && mode_ == EditMode::DrawLine && activeLine_ >= 0) {
        finishLine();
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void MapView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        if (activeLine_ >= 0) {
            finishLine();
        } else {
            pendingStation_ = -1;
            scene_->clearPreview();
            scene_->setSelectedStation(-1);
        }
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void MapView::finishLine() {
    if (activeLine_ < 0) {
        return;
    }
    const Network& network = editor_.network();
    const int line = activeLine_;
    activeLine_ = -1;
    pendingStation_ = -1;
    scene_->clearPreview();

    std::string error;
    const bool tooShort =
        line >= static_cast<int>(network.lines().size()) ||
        network.lines()[static_cast<std::size_t>(line)].stationCount() < 2;
    if (tooShort) {
        editor_.removeLine(line, &error);
        notifyStatus("Line discarded: at least two stations are required");
    } else {
        notifyStatus("Line finished");
    }
    refresh();
    notifyChanged();
}

void MapView::removeLastStop() {
    if (activeLine_ < 0) {
        notifyStatus("No line is being drawn");
        return;
    }
    std::string error;
    if (!editor_.removeLastStop(activeLine_, &error)) {
        notifyStatus(fromUtf8(error));
        return;
    }
    refresh();
    notifyChanged();
}

std::uint32_t MapView::nextLineColor() const {
    static const std::uint32_t palette[] = {
        0xef4444u, 0x2563ebu, 0x16a34au, 0xf59e0bu, 0x7c3aedu,
        0x0891b2u, 0xdb2777u, 0x65a30du, 0xea580cu, 0x4f46e5u,
    };
    const std::size_t count = sizeof(palette) / sizeof(palette[0]);
    return palette[editor_.network().lines().size() % count];
}

}  // namespace uc
