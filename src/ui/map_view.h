#pragma once

#include <QGraphicsView>
#include <QPoint>
#include <QPointF>
#include <QString>

#include <cstdint>
#include <functional>
#include <string>

class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

namespace uc {

class Editor;
class MapScene;
class StationItem;

enum class EditMode {
    Select,
    AddStation,
    AddEdge,
    DrawLine,
    Delete,
};

class MapView : public QGraphicsView {
public:
    explicit MapView(Editor& editor, QWidget* parent = nullptr);

    void setMode(EditMode mode);
    EditMode mode() const { return mode_; }

    void refresh();
    void fitContent();
    void finishLine();
    void removeLastStop();

    void selectLine(int lineIndex);
    int selectedLine() const { return selectedLine_; }
    bool hasActiveLine() const { return activeLine_ >= 0; }

    void setStatusCallback(std::function<void(const QString&)> callback);
    void setChangeCallback(std::function<void()> callback);
    void setEditLineCallback(std::function<void(int)> callback);
    void setSelectionCallback(std::function<void(int)> callback);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    StationItem* stationAt(const QPoint& viewPosition) const;
    void handleLeftPress(const QPoint& viewPosition);
    void createStationAt(const QPointF& scenePos);
    void renameStationDialog(int stationIndex);
    void deleteStation(int stationIndex);
    void deleteLine(int lineIndex);
    void deleteEdge(int edgeIndex);
    void editEdgeDialog(int edgeIndex);
    void clearSelection();
    void updateCursor();
    void showContextMenu(const QPoint& viewPosition, const QPoint& globalPosition);
    void notifyStatus(const QString& message);
    void notifyChanged();
    void notifySelectionChanged();
    std::uint32_t nextLineColor() const;

    Editor& editor_;
    MapScene* scene_ = nullptr;
    EditMode mode_ = EditMode::Select;

    bool dragging_ = false;
    bool dragMoved_ = false;
    int dragStation_ = -1;
    QPointF dragStartScene_;
    QPointF dragStationStart_;

    bool panning_ = false;
    QPoint lastPan_;

    int pendingStation_ = -1;
    int activeLine_ = -1;
    int selectedLine_ = -1;
    QPointF previewPos_;

    std::function<void(const QString&)> statusCallback_;
    std::function<void()> changeCallback_;
    std::function<void(int)> editLineCallback_;
    std::function<void(int)> selectionCallback_;
};

}  // namespace uc
