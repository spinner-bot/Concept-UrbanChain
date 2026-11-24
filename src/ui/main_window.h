#pragma once

#include <QMainWindow>
#include <QString>

#include "edit/editor.h"
#include "ui/map_view.h"

class QAction;
class QCloseEvent;
class QLabel;

namespace uc {

class MainWindow : public QMainWindow {
public:
    MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void connectCallbacks();

    void loadDemoIfAvailable();
    void newDocument();
    void openDocument();
    bool saveDocument();
    bool saveDocumentAs();
    bool saveTo(const QString& mapPath, const QString& linePath);
    bool maybeSave();

    void setMode(EditMode mode);
    void updateTitle();
    void updateActions();

    Editor editor_;
    MapView* view_ = nullptr;
    QString mapPath_;
    QString linePath_;

    QAction* newAction_ = nullptr;
    QAction* openAction_ = nullptr;
    QAction* saveAction_ = nullptr;
    QAction* saveAsAction_ = nullptr;
    QAction* exitAction_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;
    QAction* selectAction_ = nullptr;
    QAction* addStationAction_ = nullptr;
    QAction* addEdgeAction_ = nullptr;
    QAction* drawLineAction_ = nullptr;
    QAction* deleteLineAction_ = nullptr;
    QAction* finishLineAction_ = nullptr;
    QAction* removeLastStopAction_ = nullptr;
    QAction* fitAction_ = nullptr;
    QAction* aboutAction_ = nullptr;
    QLabel* modeLabel_ = nullptr;
};

}  // namespace uc
