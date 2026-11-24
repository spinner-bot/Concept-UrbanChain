#pragma once

#include <QMainWindow>
#include <QString>

#include "edit/editor.h"
#include "ui/language.h"
#include "ui/map_view.h"

class QAction;
class QCloseEvent;
class QComboBox;
class QDockWidget;
class QGroupBox;
class QShowEvent;
class QLabel;
class QListWidget;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QTimeEdit;
class QToolBar;

namespace uc {

class MainWindow : public QMainWindow {
public:
    MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void createSidePanel();
    void connectCallbacks();

    void loadDemoIfAvailable();
    void newDocument();
    void openDocument();
    bool saveDocument();
    bool saveDocumentAs();
    bool saveTo(const QString& mapPath, const QString& linePath);
    bool maybeSave();

    void setMode(EditMode mode);
    void applyLanguage(Language language);
    void retranslateUi();
    void updateTitle();
    void updateActions();
    void refreshPanels();
    void refreshLineList();
    void refreshStationCombos();
    void syncLineSelection(int lineIndex);
    int currentLineFromList() const;
    void editLine(int lineIndex);
    void deleteLine(int lineIndex);
    void editTransfers();
    void computeRoute();
    QString stationDisplay(int stationIndex) const;

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
    QAction* deleteAction_ = nullptr;
    QAction* finishLineAction_ = nullptr;
    QAction* removeLastStopAction_ = nullptr;
    QAction* transferAction_ = nullptr;
    QAction* fitAction_ = nullptr;
    QAction* aboutAction_ = nullptr;
    QAction* chineseAction_ = nullptr;
    QAction* englishAction_ = nullptr;

    QMenu* fileMenu_ = nullptr;
    QMenu* editMenu_ = nullptr;
    QMenu* modeMenu_ = nullptr;
    QMenu* lineMenu_ = nullptr;
    QMenu* viewMenu_ = nullptr;
    QMenu* settingsMenu_ = nullptr;
    QMenu* languageMenu_ = nullptr;
    QMenu* helpMenu_ = nullptr;
    QDockWidget* networkDock_ = nullptr;
    QGroupBox* linesGroup_ = nullptr;
    QGroupBox* routeGroup_ = nullptr;
    QToolBar* toolbar_ = nullptr;
    QLabel* fromLabel_ = nullptr;
    QLabel* toLabel_ = nullptr;
    QLabel* departLabel_ = nullptr;

    QLabel* modeLabel_ = nullptr;
    QListWidget* lineList_ = nullptr;
    QPushButton* editLineButton_ = nullptr;
    QPushButton* deleteLineButton_ = nullptr;
    QPushButton* transferButton_ = nullptr;
    QComboBox* fromCombo_ = nullptr;
    QComboBox* toCombo_ = nullptr;
    QTimeEdit* timeEdit_ = nullptr;
    QPushButton* routeButton_ = nullptr;
    QPlainTextEdit* routeOutput_ = nullptr;
    bool updatingPanels_ = false;
    bool firstShow_ = true;
};

}  // namespace uc
