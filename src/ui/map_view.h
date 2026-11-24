#pragma once

#include <QGraphicsView>
#include <QPoint>
#include <QPointF>
#include <QString>

#include <cstdint>
#include <functional>
#include <string>

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
    DeleteLine,
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

    void setStatusCallback(std::function<void(const QString&)> callback);
    void setChangeCallback(std::function<void()> callback);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    StationItem* stationAt(const QPoint& viewPosition) const;
    void handleLeftPress(const QPoint& viewPosition);
    void notifyStatus(const QString& message);
    void notifyChanged();
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
    QPointF previewPos_;

    std::function<void(const QString&)> statusCallback_;
    std::function<void()> changeCallback_;
};

}  // namespace uc
