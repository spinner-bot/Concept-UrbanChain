#include "ui/map_view.h"

#include <QAction>
#include <QColor>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

#include "core/network.h"
#include "edit/editor.h"
#include "ui/language.h"
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
        case EditMode::Delete:
            return "Delete";
    }
    return "Unknown";
}

QString fromUtf8(const std::string& text) {
    return localizeMessage(QString::fromUtf8(text.c_str()));
}

double hitThreshold(const QGraphicsView& view, double pixels) {
    const double scale = std::max(0.0001, view.transform().m11());
    return pixels / scale;
}

}  // namespace

MapView::MapView(Editor& editor, QWidget* parent) : QGraphicsView(parent), editor_(editor) {
    scene_ = new MapScene(editor_, this);
    setObjectName("mapView");
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
    updateCursor();
}

void MapView::setStatusCallback(std::function<void(const QString&)> callback) {
    statusCallback_ = static_cast<std::function<void(const QString&)>&&>(callback);
}

void MapView::setChangeCallback(std::function<void()> callback) {
    changeCallback_ = static_cast<std::function<void()>&&>(callback);
}

void MapView::setEditLineCallback(std::function<void(int)> callback) {
    editLineCallback_ = static_cast<std::function<void(int)>&&>(callback);
}

void MapView::setSelectionCallback(std::function<void(int)> callback) {
    selectionCallback_ = static_cast<std::function<void(int)>&&>(callback);
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

void MapView::notifySelectionChanged() {
    if (selectionCallback_) {
        selectionCallback_(selectedLine_);
    }
}

void MapView::updateCursor() {
    if (panning_) {
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    switch (mode_) {
        case EditMode::Select:
            setCursor(Qt::ArrowCursor);
            break;
        case EditMode::AddStation:
        case EditMode::DrawLine:
        case EditMode::Delete:
            setCursor(Qt::CrossCursor);
            break;
        case EditMode::AddEdge:
            setCursor(Qt::PointingHandCursor);
            break;
    }
}

void MapView::selectLine(int lineIndex) {
    if (lineIndex < -1 || lineIndex >= static_cast<int>(editor_.network().lines().size())) {
        lineIndex = -1;
    }
    selectedLine_ = lineIndex;
    scene_->setSelectedLine(lineIndex);
    if (lineIndex >= 0) {
        scene_->setSelectedStation(-1);
    }
    notifySelectionChanged();
}

void MapView::clearSelection() {
    selectedLine_ = -1;
    scene_->setSelectedLine(-1);
    scene_->setSelectedStation(-1);
    notifySelectionChanged();
}

void MapView::refresh() {
    scene_->refresh();
    if (selectedLine_ >= static_cast<int>(editor_.network().lines().size())) {
        selectedLine_ = -1;
        notifySelectionChanged();
    }
    scene_->setSelectedLine(selectedLine_);
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
    }
    mode_ = mode;
    scene_->clearPreview();
    updateCursor();
    notifyStatus(L("Mode: %1").arg(L(modeName(mode_))));
}

void MapView::fitContent() {
    if (scene_->sceneRect().isEmpty()) {
        resetTransform();
        return;
    }
    fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
}

void MapView::drawBackground(QPainter* painter, const QRectF& rect) {
    QGraphicsView::drawBackground(painter, rect);
    scene_->paintNetwork(painter, rect);
}

void MapView::drawForeground(QPainter* painter, const QRectF& rect) {
    scene_->paintOverlay(painter, rect);
    QGraphicsView::drawForeground(painter, rect);
}

void MapView::wheelEvent(QWheelEvent* event) {
    const double factor = std::pow(1.0015, static_cast<double>(event->angleDelta().y()));
    const double next = transform().m11() * factor;
    if (next < 0.001 || next > 1000.0) {
        event->accept();
        return;
    }
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

void MapView::createStationAt(const QPointF& scenePos) {
    bool accepted = false;
    const QString name = QInputDialog::getText(this, L("New station"), L("Station name:"),
                                               QLineEdit::Normal, QString(), &accepted);
    if (!accepted) {
        return;
    }
    std::string error;
    const Index index = editor_.addStation(name.toUtf8().constData(), scenePos.x(),
                                           -scenePos.y(), &error);
    if (index == kInvalidIndex) {
        notifyStatus(fromUtf8(error));
        return;
    }
    refresh();
    scene_->setSelectedStation(index);
    selectedLine_ = -1;
    scene_->setSelectedLine(-1);
    notifySelectionChanged();
    notifyStatus(L("Station added"));
    notifyChanged();
}

void MapView::renameStationDialog(int stationIndex) {
    if (stationIndex < 0 || stationIndex >= editor_.network().map().stationCount()) {
        return;
    }
    const Station& station = editor_.network().map().station(stationIndex);
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, L("Rename station"), L("Station name:"), QLineEdit::Normal,
        QString::fromUtf8(station.name.c_str()), &accepted);
    if (!accepted || name.trimmed().isEmpty()) {
        return;
    }
    std::string error;
    if (!editor_.renameStation(stationIndex, name.toUtf8().constData(), &error)) {
        notifyStatus(fromUtf8(error));
        return;
    }
    refresh();
    notifyStatus(L("Station renamed"));
    notifyChanged();
}

void MapView::deleteStation(int stationIndex) {
    std::string error;
    if (!editor_.removeStation(stationIndex, &error)) {
        notifyStatus(fromUtf8(error));
        return;
    }
    scene_->setSelectedStation(-1);
    refresh();
    notifyStatus(L("Station deleted"));
    notifyChanged();
}

void MapView::deleteLine(int lineIndex) {
    std::string error;
    if (!editor_.removeLine(lineIndex, &error)) {
        notifyStatus(fromUtf8(error));
        return;
    }
    selectedLine_ = -1;
    scene_->setSelectedLine(-1);
    refresh();
    notifySelectionChanged();
    notifyStatus(L("Line deleted"));
    notifyChanged();
}

void MapView::editEdgeDialog(int edgeIndex) {
    if (edgeIndex < 0 ||
        edgeIndex >= static_cast<int>(editor_.network().map().edges().size())) {
        return;
    }
    const EdgeRecord edge =
        editor_.network().map().edges()[static_cast<std::size_t>(edgeIndex)];

    QDialog dialog(this);
    dialog.setWindowTitle(L("Edge Properties"));
    auto* layout = new QFormLayout(&dialog);
    auto* costSpin = new QDoubleSpinBox(&dialog);
    costSpin->setRange(0.0, 1.0e9);
    costSpin->setDecimals(4);
    costSpin->setValue(edge.unitCost);
    auto* lengthSpin = new QDoubleSpinBox(&dialog);
    lengthSpin->setRange(0.0, 1.0e9);
    lengthSpin->setDecimals(3);
    lengthSpin->setValue(edge.length);
    layout->addRow(L("Unit Cost"), costSpin);
    layout->addRow(L("Length"), lengthSpin);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    std::string error;
    if (!editor_.setEdgeWeight(edge.from, edge.to, costSpin->value(), lengthSpin->value(),
                               &error)) {
        notifyStatus(fromUtf8(error));
        return;
    }
    refresh();
    notifyStatus(L("Edge updated"));
    notifyChanged();
}

void MapView::deleteEdge(int edgeIndex) {
    if (edgeIndex < 0 ||
        edgeIndex >= static_cast<int>(editor_.network().map().edges().size())) {
        return;
    }
    const EdgeRecord edge = editor_.network().map().edges()[static_cast<std::size_t>(edgeIndex)];
    std::string error;
    if (!editor_.removeEdge(edge.from, edge.to, &error)) {
        notifyStatus(fromUtf8(error));
        return;
    }
    refresh();
    notifyStatus(L("Edge deleted"));
    notifyChanged();
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
                if (selectedLine_ != -1) {
                    selectedLine_ = -1;
                    scene_->setSelectedLine(-1);
                    notifySelectionChanged();
                }
                dragging_ = true;
                dragMoved_ = false;
                dragStation_ = index;
                dragStartScene_ = scenePos;
                dragStationStart_ = station->pos();
                scene_->setDraggingStation(index, dragStationStart_);
            } else {
                const int line = scene_->lineAt(scenePos, hitThreshold(*this, 12.0));
                if (line >= 0) {
                    selectLine(line);
                } else {
                    clearSelection();
                }
            }
            break;
        }
        case EditMode::AddStation: {
            if (station != nullptr) {
                notifyStatus(L("Click an empty area to place a station"));
                break;
            }
            createStationAt(scenePos);
            break;
        }
        case EditMode::AddEdge: {
            if (station == nullptr) {
                pendingStation_ = -1;
                scene_->clearPreview();
                notifyStatus(L("Click the first station"));
                break;
            }
            if (pendingStation_ < 0) {
                pendingStation_ = station->stationIndex();
                scene_->setPreview(scenePos, pendingStation_, -1);
                notifyStatus(L("Click the second station"));
                break;
            }
            const int first = pendingStation_;
            const int second = station->stationIndex();
            pendingStation_ = -1;
            if (first == second) {
                scene_->clearPreview();
                notifyStatus(L("Choose a different station"));
                break;
            }
            if (!editor_.addEdge(first, second, &error)) {
                notifyStatus(fromUtf8(error));
                break;
            }
            refresh();
            notifyStatus(L("Edge added"));
            notifyChanged();
            break;
        }
        case EditMode::DrawLine: {
            if (station == nullptr) {
                notifyStatus(L("Click a station to add it to the line"));
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
            selectedLine_ = activeLine_;
            refresh();
            notifySelectionChanged();
            notifyStatus(L("Station added to line; double-click or right-click to finish"));
            notifyChanged();
            break;
        }
        case EditMode::Delete: {
            if (station != nullptr) {
                deleteStation(station->stationIndex());
                break;
            }
            const int line = scene_->lineAt(scenePos, hitThreshold(*this, 12.0));
            if (line >= 0) {
                deleteLine(line);
                break;
            }
            const int edge = scene_->edgeAt(scenePos, hitThreshold(*this, 10.0));
            if (edge >= 0) {
                deleteEdge(edge);
                break;
            }
            notifyStatus(L("Nothing to delete here"));
            break;
        }
    }
}

void MapView::showContextMenu(const QPoint& viewPosition, const QPoint& globalPosition) {
    const QPointF scenePos = mapToScene(viewPosition);
    StationItem* station = stationAt(viewPosition);
    const int line = station == nullptr
                         ? scene_->lineAt(scenePos, hitThreshold(*this, 12.0))
                         : -1;
    const int edge = station == nullptr && line < 0
                         ? scene_->edgeAt(scenePos, hitThreshold(*this, 10.0))
                         : -1;

    QMenu menu(this);
    if (station != nullptr) {
        const int stationIndex = station->stationIndex();
        QAction* renameAction = menu.addAction(L("Rename Station..."));
        QAction* deleteAction = menu.addAction(L("Delete Station"));
        QAction* chosen = menu.exec(globalPosition);
        if (chosen == renameAction) {
            renameStationDialog(stationIndex);
        } else if (chosen == deleteAction) {
            deleteStation(stationIndex);
        }
        return;
    }
    if (line >= 0) {
        QAction* editAction = menu.addAction(L("Edit Line..."));
        QAction* deleteAction = menu.addAction(L("Delete Line"));
        QAction* chosen = menu.exec(globalPosition);
        if (chosen == editAction) {
            if (editLineCallback_) {
                editLineCallback_(line);
            }
        } else if (chosen == deleteAction) {
            deleteLine(line);
        }
        return;
    }
    if (edge >= 0) {
        QAction* editAction = menu.addAction(L("Edit Edge..."));
        QAction* deleteAction = menu.addAction(L("Delete Edge"));
        QAction* chosen = menu.exec(globalPosition);
        if (chosen == editAction) {
            editEdgeDialog(edge);
        } else if (chosen == deleteAction) {
            deleteEdge(edge);
        }
        return;
    }

    QAction* addAction = menu.addAction(L("Add Station Here"));
    if (menu.exec(globalPosition) == addAction) {
        createStationAt(scenePos);
    }
}

void MapView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        lastPan_ = event->pos();
        updateCursor();
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
        const QPointF visualPosition = dragStationStart_ + delta;
        scene_->setStationVisualPosition(dragStation_, visualPosition);
        scene_->setDraggingStation(dragStation_, visualPosition);
        event->accept();
        return;
    }
    if (pendingStation_ >= 0 || activeLine_ >= 0) {
        previewPos_ = mapToScene(event->pos());
        scene_->setPreview(previewPos_, pendingStation_, activeLine_);
    }
    event->accept();
}

void MapView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && panning_) {
        panning_ = false;
        updateCursor();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        scene_->clearDraggingStation();
        if (dragMoved_) {
            const QPointF scenePos = mapToScene(event->pos());
            const QPointF delta = scenePos - dragStartScene_;
            const QPointF target = dragStationStart_ + delta;
            std::string error;
            if (!editor_.moveStation(dragStation_, target.x(), -target.y(), &error)) {
                notifyStatus(fromUtf8(error));
            } else {
                notifyStatus(L("Station moved"));
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
    if (event->button() == Qt::LeftButton) {
        if (mode_ == EditMode::DrawLine && activeLine_ >= 0) {
            finishLine();
            event->accept();
            return;
        }
        if (mode_ == EditMode::Select) {
            if (StationItem* station = stationAt(event->pos())) {
                renameStationDialog(station->stationIndex());
                event->accept();
                return;
            }
            const int line = scene_->lineAt(mapToScene(event->pos()), hitThreshold(*this, 12.0));
            if (line >= 0 && editLineCallback_) {
                editLineCallback_(line);
                event->accept();
                return;
            }
        }
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
            clearSelection();
        }
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void MapView::contextMenuEvent(QContextMenuEvent* event) {
    if (activeLine_ >= 0) {
        finishLine();
        event->accept();
        return;
    }
    if (pendingStation_ >= 0) {
        pendingStation_ = -1;
        scene_->clearPreview();
        notifyStatus(L("Edge creation cancelled"));
        event->accept();
        return;
    }
    showContextMenu(event->pos(), event->globalPos());
    event->accept();
}

void MapView::finishLine() {
    if (activeLine_ < 0) {
        return;
    }
    const int line = activeLine_;
    const bool tooShort =
        line >= static_cast<int>(editor_.network().lines().size()) ||
        editor_.network().lines()[static_cast<std::size_t>(line)].stationCount() < 2;
    activeLine_ = -1;
    pendingStation_ = -1;
    scene_->clearPreview();

    std::string error;
    if (tooShort) {
        editor_.removeLine(line, &error);
        selectedLine_ = -1;
        notifyStatus(L("Line discarded: at least two stations are required"));
    } else {
        selectedLine_ = line;
        notifyStatus(L("Line finished"));
    }
    refresh();
    notifySelectionChanged();
    notifyChanged();
}

void MapView::removeLastStop() {
    if (activeLine_ < 0) {
        notifyStatus(L("No line is being drawn"));
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
