#include "ui/main_window.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTime>
#include <QTimeEdit>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include <cmath>
#include <string>

#include "nav/router.h"
#include "ui/line_dialog.h"
#include "ui/transfer_dialog.h"

namespace uc {
namespace {

QString formatClock(double seconds) {
    const int minutes = static_cast<int>(std::floor(seconds / 60.0 + 0.5));
    const int hours = minutes / 60;
    const int rest = minutes % 60;
    return QString("%1:%2")
        .arg(hours, 2, 10, QChar('0'))
        .arg(rest, 2, 10, QChar('0'));
}

QColor lineColor(std::uint32_t rgb) {
    return QColor::fromRgb(static_cast<QRgb>(rgb | 0xff000000u));
}

QString stationName(const Network& network, Index station) {
    if (station < 0 || station >= network.map().stationCount()) {
        return "?";
    }
    return QString::fromUtf8(network.map().station(station).name.c_str());
}

}  // namespace

MainWindow::MainWindow() {
    view_ = new MapView(editor_, this);
    setCentralWidget(view_);
    resize(1280, 840);

    createActions();
    createMenus();
    createToolBar();
    createStatusBar();
    createSidePanel();
    connectCallbacks();

    updateTitle();
    updateActions();
    refreshPanels();
    loadDemoIfAvailable();
    view_->fitContent();
}

void MainWindow::createActions() {
    newAction_ = new QAction(L("&New"), this);
    newAction_->setObjectName("newAction");
    newAction_->setShortcut(QKeySequence::New);
    connect(newAction_, &QAction::triggered, this, [this] { newDocument(); });

    openAction_ = new QAction(L("&Open..."), this);
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, this, [this] { openDocument(); });

    saveAction_ = new QAction(L("&Save"), this);
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, [this] { saveDocument(); });

    saveAsAction_ = new QAction(L("Save &As..."), this);
    saveAsAction_->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction_, &QAction::triggered, this, [this] { saveDocumentAs(); });

    exitAction_ = new QAction(L("E&xit"), this);
    exitAction_->setShortcut(QKeySequence::Quit);
    connect(exitAction_, &QAction::triggered, this, [this] { close(); });

    undoAction_ = new QAction(L("&Undo"), this);
    undoAction_->setObjectName("undoAction");
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, this, [this] {
        if (editor_.undo()) {
            view_->refresh();
            updateTitle();
            updateActions();
            refreshPanels();
            statusBar()->showMessage(L("Undo"), 2000);
        }
    });

    redoAction_ = new QAction(L("&Redo"), this);
    redoAction_->setShortcut(QKeySequence::Redo);
    connect(redoAction_, &QAction::triggered, this, [this] {
        if (editor_.redo()) {
            view_->refresh();
            updateTitle();
            updateActions();
            refreshPanels();
            statusBar()->showMessage(L("Redo"), 2000);
        }
    });

    selectAction_ = new QAction(L("&Select / Move"), this);
    selectAction_->setCheckable(true);
    selectAction_->setChecked(true);
    connect(selectAction_, &QAction::triggered, this, [this] { setMode(EditMode::Select); });

    addStationAction_ = new QAction(L("Add &Station"), this);
    addStationAction_->setObjectName("addStationAction");
    addStationAction_->setCheckable(true);
    connect(addStationAction_, &QAction::triggered, this, [this] { setMode(EditMode::AddStation); });

    addEdgeAction_ = new QAction(L("Add &Edge"), this);
    addEdgeAction_->setObjectName("addEdgeAction");
    addEdgeAction_->setCheckable(true);
    connect(addEdgeAction_, &QAction::triggered, this, [this] { setMode(EditMode::AddEdge); });

    drawLineAction_ = new QAction(L("&Draw Line"), this);
    drawLineAction_->setObjectName("drawLineAction");
    drawLineAction_->setCheckable(true);
    connect(drawLineAction_, &QAction::triggered, this, [this] { setMode(EditMode::DrawLine); });

    deleteAction_ = new QAction(L("&Delete"), this);
    deleteAction_->setObjectName("deleteAction");
    deleteAction_->setCheckable(true);
    connect(deleteAction_, &QAction::triggered, this, [this] { setMode(EditMode::Delete); });

    auto* modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addAction(selectAction_);
    modeGroup->addAction(addStationAction_);
    modeGroup->addAction(addEdgeAction_);
    modeGroup->addAction(drawLineAction_);
    modeGroup->addAction(deleteAction_);

    finishLineAction_ = new QAction(L("&Finish Line"), this);
    finishLineAction_->setObjectName("finishLineAction");
    finishLineAction_->setShortcut(QKeySequence(Qt::Key_Return));
    connect(finishLineAction_, &QAction::triggered, this, [this] {
        view_->finishLine();
        updateTitle();
        updateActions();
        refreshPanels();
    });

    removeLastStopAction_ = new QAction(L("Remove Last &Stop"), this);
    removeLastStopAction_->setShortcut(QKeySequence(Qt::Key_Backspace));
    connect(removeLastStopAction_, &QAction::triggered, this, [this] {
        view_->removeLastStop();
        updateTitle();
        updateActions();
        refreshPanels();
    });

    transferAction_ = new QAction(L("Transfer &Times..."), this);
    transferAction_->setObjectName("transferAction");
    connect(transferAction_, &QAction::triggered, this, [this] { editTransfers(); });

    fitAction_ = new QAction(L("&Fit Content"), this);
    fitAction_->setShortcut(QKeySequence(Qt::Key_F));
    connect(fitAction_, &QAction::triggered, this, [this] { view_->fitContent(); });

    aboutAction_ = new QAction(L("&About"), this);
    connect(aboutAction_, &QAction::triggered, this, [this] {
        QMessageBox::about(this, L("About Concept UrbanChain"),
                           L("Concept UrbanChain v0.3\n"
                              "A data-structure course project for metro network editing and "
                              "time-dependent routing."));
    });

    chineseAction_ = new QAction(QString::fromUtf8("中文"), this);
    chineseAction_->setObjectName("chineseLanguageAction");
    chineseAction_->setCheckable(true);
    connect(chineseAction_, &QAction::triggered, this,
            [this] { applyLanguage(Language::Chinese); });

    englishAction_ = new QAction(QString("English"), this);
    englishAction_->setObjectName("englishLanguageAction");
    englishAction_->setCheckable(true);
    connect(englishAction_, &QAction::triggered, this,
            [this] { applyLanguage(Language::English); });

    auto* languageGroup = new QActionGroup(this);
    languageGroup->setExclusive(true);
    languageGroup->addAction(chineseAction_);
    languageGroup->addAction(englishAction_);
    chineseAction_->setChecked(currentLanguage() == Language::Chinese);
    englishAction_->setChecked(currentLanguage() == Language::English);
}

void MainWindow::createMenus() {
    fileMenu_ = menuBar()->addMenu(L("&File"));
    fileMenu_->setObjectName("fileMenu");
    fileMenu_->addAction(newAction_);
    fileMenu_->addAction(openAction_);
    fileMenu_->addAction(saveAction_);
    fileMenu_->addAction(saveAsAction_);
    fileMenu_->addSeparator();
    fileMenu_->addAction(exitAction_);

    editMenu_ = menuBar()->addMenu(L("&Edit"));
    editMenu_->addAction(undoAction_);
    editMenu_->addAction(redoAction_);

    modeMenu_ = menuBar()->addMenu(L("&Mode"));
    modeMenu_->addAction(selectAction_);
    modeMenu_->addAction(addStationAction_);
    modeMenu_->addAction(addEdgeAction_);
    modeMenu_->addAction(drawLineAction_);
    modeMenu_->addAction(deleteAction_);

    lineMenu_ = menuBar()->addMenu(L("&Line"));
    lineMenu_->addAction(finishLineAction_);
    lineMenu_->addAction(removeLastStopAction_);
    lineMenu_->addSeparator();
    lineMenu_->addAction(transferAction_);

    viewMenu_ = menuBar()->addMenu(L("&View"));
    viewMenu_->addAction(fitAction_);

    settingsMenu_ = menuBar()->addMenu(L("&Settings"));
    settingsMenu_->setObjectName("settingsMenu");
    languageMenu_ = settingsMenu_->addMenu(L("&Language"));
    languageMenu_->addAction(chineseAction_);
    languageMenu_->addAction(englishAction_);

    helpMenu_ = menuBar()->addMenu(L("&Help"));
    helpMenu_->addAction(aboutAction_);
}

void MainWindow::createToolBar() {
    toolbar_ = addToolBar(L("Main"));
    toolbar_->setMovable(false);
    toolbar_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar_->addAction(openAction_);
    toolbar_->addAction(saveAction_);
    toolbar_->addSeparator();
    toolbar_->addAction(undoAction_);
    toolbar_->addAction(redoAction_);
    toolbar_->addSeparator();
    toolbar_->addAction(selectAction_);
    toolbar_->addAction(addStationAction_);
    toolbar_->addAction(addEdgeAction_);
    toolbar_->addAction(drawLineAction_);
    toolbar_->addAction(deleteAction_);
    toolbar_->addSeparator();
    toolbar_->addAction(finishLineAction_);
}

void MainWindow::createStatusBar() {
    statusBar()->showMessage(L("Ready"));
    modeLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(modeLabel_);
}

void MainWindow::createSidePanel() {
    networkDock_ = new QDockWidget(L("Network"), this);
    networkDock_->setObjectName("networkDock");
    networkDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* panel = new QWidget(networkDock_);
    panel->setMinimumWidth(320);
    auto* layout = new QVBoxLayout(panel);

    linesGroup_ = new QGroupBox(L("Lines"), panel);
    auto* linesLayout = new QVBoxLayout(linesGroup_);
    lineList_ = new QListWidget(linesGroup_);
    lineList_->setObjectName("lineList");
    lineList_->setMinimumHeight(150);
    linesLayout->addWidget(lineList_);
    auto* lineButtons = new QHBoxLayout();
    editLineButton_ = new QPushButton(L("Edit..."), linesGroup_);
    editLineButton_->setObjectName("editLineButton");
    deleteLineButton_ = new QPushButton(L("Delete"), linesGroup_);
    transferButton_ = new QPushButton(L("Transfers..."), linesGroup_);
    transferButton_->setObjectName("transferButton");
    lineButtons->addWidget(editLineButton_);
    lineButtons->addWidget(deleteLineButton_);
    lineButtons->addWidget(transferButton_);
    linesLayout->addLayout(lineButtons);
    layout->addWidget(linesGroup_);

    routeGroup_ = new QGroupBox(L("Route"), panel);
    auto* routeLayout = new QVBoxLayout(routeGroup_);
    auto* routeForm = new QFormLayout();
    fromCombo_ = new QComboBox(routeGroup_);
    fromCombo_->setObjectName("fromCombo");
    toCombo_ = new QComboBox(routeGroup_);
    toCombo_->setObjectName("toCombo");
    timeEdit_ = new QTimeEdit(QTime(8, 0), routeGroup_);
    timeEdit_->setObjectName("timeEdit");
    timeEdit_->setDisplayFormat("HH:mm");
    fromLabel_ = new QLabel(L("From"), routeGroup_);
    toLabel_ = new QLabel(L("To"), routeGroup_);
    departLabel_ = new QLabel(L("Depart"), routeGroup_);
    routeForm->addRow(fromLabel_, fromCombo_);
    routeForm->addRow(toLabel_, toCombo_);
    routeForm->addRow(departLabel_, timeEdit_);
    routeLayout->addLayout(routeForm);
    routeButton_ = new QPushButton(L("Find Route"), routeGroup_);
    routeButton_->setObjectName("routeButton");
    routeLayout->addWidget(routeButton_);
    routeOutput_ = new QPlainTextEdit(routeGroup_);
    routeOutput_->setObjectName("routeOutput");
    routeOutput_->setReadOnly(true);
    routeOutput_->setMinimumHeight(180);
    routeOutput_->setPlaceholderText(L("Route details appear here"));
    routeLayout->addWidget(routeOutput_, 1);
    layout->addWidget(routeGroup_, 1);

    networkDock_->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, networkDock_);

    connect(lineList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (updatingPanels_) {
            return;
        }
        if (row < 0) {
            view_->selectLine(-1);
        } else {
            const int line = lineList_->item(row)->data(Qt::UserRole).toInt();
            view_->selectLine(line);
        }
        updateActions();
    });
    connect(editLineButton_, &QPushButton::clicked, this, [this] {
        const int line = currentLineFromList();
        if (line >= 0) {
            editLine(line);
        }
    });
    connect(deleteLineButton_, &QPushButton::clicked, this, [this] {
        const int line = currentLineFromList();
        if (line >= 0) {
            deleteLine(line);
        }
    });
    connect(transferButton_, &QPushButton::clicked, this, [this] { editTransfers(); });
    connect(routeButton_, &QPushButton::clicked, this, [this] { computeRoute(); });
}

void MainWindow::connectCallbacks() {
    view_->setStatusCallback([this](const QString& message) {
        statusBar()->showMessage(message, 4000);
    });
    view_->setChangeCallback([this] {
        updateTitle();
        updateActions();
        refreshPanels();
    });
    view_->setEditLineCallback([this](int line) { editLine(line); });
    view_->setSelectionCallback([this](int line) { syncLineSelection(line); });
}

void MainWindow::loadDemoIfAvailable() {
    const QString bases[] = {"data/demo", "../data/demo", "../../data/demo"};
    for (const QString& base : bases) {
        const QString mapPath = base + ".cuc3m";
        const QString linePath = base + ".cuc3l";
        if (!QFileInfo::exists(mapPath) || !QFileInfo::exists(linePath)) {
            continue;
        }
        std::string error;
        if (editor_.load(mapPath.toUtf8().constData(), linePath.toUtf8().constData(), &error)) {
            mapPath_ = mapPath;
            linePath_ = linePath;
            view_->refresh();
            updateTitle();
            updateActions();
            refreshPanels();
            if (!editor_.network().lines().empty()) {
                view_->selectLine(0);
            }
            statusBar()->showMessage(L("Loaded demo network"), 3000);
            return;
        }
    }
}

void MainWindow::newDocument() {
    if (!maybeSave()) {
        return;
    }
    editor_.newDocument();
    mapPath_.clear();
    linePath_.clear();
    view_->refresh();
    view_->fitContent();
    updateTitle();
    updateActions();
    refreshPanels();
    statusBar()->showMessage(L("New document"), 3000);
}

void MainWindow::openDocument() {
    if (!maybeSave()) {
        return;
    }
    const QString mapPath = QFileDialog::getOpenFileName(
        this, L("Open map"), QString(), L("UrbanChain map (*.cuc3m);;All files (*)"));
    if (mapPath.isEmpty()) {
        return;
    }

    // 优先使用与地图同名的线路文件，避免每次打开都要选两个文件。
    QString linePath;
    const QFileInfo mapInfo(mapPath);
    const QString sibling =
        mapInfo.dir().filePath(mapInfo.completeBaseName() + QString(".cuc3l"));
    if (QFileInfo::exists(sibling)) {
        linePath = sibling;
    } else {
        linePath = QFileDialog::getOpenFileName(
            this, L("Open line file"), mapInfo.dir().absolutePath(),
            L("UrbanChain lines (*.cuc3l);;All files (*)"));
    }
    if (linePath.isEmpty()) {
        return;
    }

    std::string error;
    if (!editor_.load(mapPath.toUtf8().constData(), linePath.toUtf8().constData(), &error)) {
        QMessageBox::critical(this, L("Open failed"), localizeMessage(QString::fromUtf8(error.c_str())));
        return;
    }
    mapPath_ = mapPath;
    linePath_ = linePath;
    view_->refresh();
    view_->fitContent();
    updateTitle();
    updateActions();
    refreshPanels();
    if (!editor_.network().lines().empty()) {
        view_->selectLine(0);
    }
    statusBar()->showMessage(L("Loaded"), 3000);
}

bool MainWindow::saveDocument() {
    if (mapPath_.isEmpty() || linePath_.isEmpty()) {
        return saveDocumentAs();
    }
    return saveTo(mapPath_, linePath_);
}

bool MainWindow::saveDocumentAs() {
    const QString mapPath = QFileDialog::getSaveFileName(
        this, L("Save map"), mapPath_.isEmpty() ? QString("map.cuc3m") : mapPath_,
        L("UrbanChain map (*.cuc3m);;All files (*)"));
    if (mapPath.isEmpty()) {
        return false;
    }
    QString defaultLinePath = linePath_;
    if (defaultLinePath.isEmpty()) {
        const QFileInfo mapInfo(mapPath);
        defaultLinePath =
            mapInfo.dir().filePath(mapInfo.completeBaseName() + QString(".cuc3l"));
    }
    const QString linePath = QFileDialog::getSaveFileName(
        this, L("Save line file"), defaultLinePath,
        L("UrbanChain lines (*.cuc3l);;All files (*)"));
    if (linePath.isEmpty()) {
        return false;
    }
    return saveTo(mapPath, linePath);
}

bool MainWindow::saveTo(const QString& mapPath, const QString& linePath) {
    std::string error;
    if (!editor_.save(mapPath.toUtf8().constData(), linePath.toUtf8().constData(), &error)) {
        QMessageBox::critical(this, L("Save failed"), localizeMessage(QString::fromUtf8(error.c_str())));
        return false;
    }
    mapPath_ = mapPath;
    linePath_ = linePath;
    updateTitle();
    updateActions();
    statusBar()->showMessage(L("Saved"), 3000);
    return true;
}

bool MainWindow::maybeSave() {
    if (!editor_.isDirty()) {
        return true;
    }
    const QMessageBox::StandardButton answer = QMessageBox::warning(
        this, L("Unsaved changes"), L("Save changes before continuing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save) {
        return saveDocument();
    }
    return answer == QMessageBox::Discard;
}

void MainWindow::setMode(EditMode mode) {
    view_->setMode(mode);
    if (mode == EditMode::Select) {
        selectAction_->setChecked(true);
    } else if (mode == EditMode::AddStation) {
        addStationAction_->setChecked(true);
    } else if (mode == EditMode::AddEdge) {
        addEdgeAction_->setChecked(true);
    } else if (mode == EditMode::DrawLine) {
        drawLineAction_->setChecked(true);
    } else if (mode == EditMode::Delete) {
        deleteAction_->setChecked(true);
    }
    updateActions();
}

void MainWindow::applyLanguage(Language language) {
    if (currentLanguage() == language) {
        return;
    }
    setLanguage(language, true);
    retranslateUi();
    statusBar()->showMessage(L("Language changed"), 3000);
}

void MainWindow::retranslateUi() {
    newAction_->setText(L("&New"));
    openAction_->setText(L("&Open..."));
    saveAction_->setText(L("&Save"));
    saveAsAction_->setText(L("Save &As..."));
    exitAction_->setText(L("E&xit"));
    undoAction_->setText(L("&Undo"));
    redoAction_->setText(L("&Redo"));
    selectAction_->setText(L("&Select / Move"));
    addStationAction_->setText(L("Add &Station"));
    addEdgeAction_->setText(L("Add &Edge"));
    drawLineAction_->setText(L("&Draw Line"));
    deleteAction_->setText(L("&Delete"));
    finishLineAction_->setText(L("&Finish Line"));
    removeLastStopAction_->setText(L("Remove Last &Stop"));
    transferAction_->setText(L("Transfer &Times..."));
    fitAction_->setText(L("&Fit Content"));
    aboutAction_->setText(L("&About"));

    fileMenu_->setTitle(L("&File"));
    editMenu_->setTitle(L("&Edit"));
    modeMenu_->setTitle(L("&Mode"));
    lineMenu_->setTitle(L("&Line"));
    viewMenu_->setTitle(L("&View"));
    settingsMenu_->setTitle(L("&Settings"));
    languageMenu_->setTitle(L("&Language"));
    helpMenu_->setTitle(L("&Help"));

    toolbar_->setWindowTitle(L("Main"));
    networkDock_->setWindowTitle(L("Network"));
    linesGroup_->setTitle(L("Lines"));
    routeGroup_->setTitle(L("Route"));
    fromLabel_->setText(L("From"));
    toLabel_->setText(L("To"));
    departLabel_->setText(L("Depart"));
    editLineButton_->setText(L("Edit..."));
    deleteLineButton_->setText(L("Delete"));
    transferButton_->setText(L("Transfers..."));
    routeButton_->setText(L("Find Route"));
    routeOutput_->setPlaceholderText(L("Route details appear here"));
    chineseAction_->setChecked(currentLanguage() == Language::Chinese);
    englishAction_->setChecked(currentLanguage() == Language::English);

    updateTitle();
    refreshPanels();
    updateActions();
}

void MainWindow::updateTitle() {
    const QString name = mapPath_.isEmpty() ? L("Untitled") : QFileInfo(mapPath_).fileName();
    const QString suffix = editor_.isDirty() ? QString(" *") : QString();
    setWindowTitle(QString("Concept UrbanChain v0.3 - ") + name + suffix);
}

void MainWindow::updateActions() {
    undoAction_->setEnabled(editor_.canUndo());
    redoAction_->setEnabled(editor_.canRedo());

    QString mode = L("Select");
    if (addStationAction_->isChecked()) {
        mode = L("Add station");
    } else if (addEdgeAction_->isChecked()) {
        mode = L("Add edge");
    } else if (drawLineAction_->isChecked()) {
        mode = L("Draw line");
    } else if (deleteAction_->isChecked()) {
        mode = L("Delete");
    }
    if (modeLabel_ != nullptr) {
        modeLabel_->setText(L("%1 | %2 stations | %3 lines")
                                .arg(mode)
                                .arg(editor_.network().map().stationCount())
                                .arg(static_cast<int>(editor_.network().lines().size())));
    }
    if (editLineButton_ != nullptr) {
        const bool hasLine = currentLineFromList() >= 0;
        editLineButton_->setEnabled(hasLine);
        deleteLineButton_->setEnabled(hasLine);
        if (transferButton_ != nullptr) {
            transferButton_->setEnabled(!editor_.network().lines().empty());
        }
        routeButton_->setEnabled(fromCombo_->count() > 0 && toCombo_->count() > 0);
        if (finishLineAction_ != nullptr) {
            const bool drawing = view_->hasActiveLine();
            finishLineAction_->setEnabled(drawing);
            removeLastStopAction_->setEnabled(drawing);
        }
    }
}

void MainWindow::refreshPanels() {
    if (updatingPanels_) {
        return;
    }
    refreshLineList();
    refreshStationCombos();
    if (routeOutput_ != nullptr) {
        routeOutput_->clear();
    }
    updateActions();
}

void MainWindow::refreshLineList() {
    if (lineList_ == nullptr) {
        return;
    }
    int keep = view_->selectedLine();
    if (keep < 0) {
        keep = currentLineFromList();
    }

    updatingPanels_ = true;
    {
        QSignalBlocker blocker(lineList_);
        lineList_->clear();
        for (std::size_t i = 0; i < editor_.network().lines().size(); ++i) {
            const Line& line = editor_.network().lines()[i];
            QPixmap swatch(12, 12);
            swatch.fill(lineColor(line.color()));
            auto* item = new QListWidgetItem(
                QIcon(swatch),
                L("%1 (%2 stops)")
                    .arg(QString::fromUtf8(line.name().c_str()))
                    .arg(line.stationCount()));
            item->setData(Qt::UserRole, static_cast<int>(i));
            lineList_->addItem(item);
        }
        if (keep >= 0 && keep < static_cast<int>(editor_.network().lines().size())) {
            lineList_->setCurrentRow(keep);
        } else {
            lineList_->setCurrentRow(-1);
        }
    }
    updatingPanels_ = false;

    const bool hasLine = currentLineFromList() >= 0;
    if (editLineButton_ != nullptr) {
        editLineButton_->setEnabled(hasLine);
        deleteLineButton_->setEnabled(hasLine);
    }
}

void MainWindow::refreshStationCombos() {
    if (fromCombo_ == nullptr || toCombo_ == nullptr) {
        return;
    }
    const QString keepFrom =
        fromCombo_->currentData().isValid() ? fromCombo_->currentData().toString() : QString();
    const QString keepTo =
        toCombo_->currentData().isValid() ? toCombo_->currentData().toString() : QString();

    updatingPanels_ = true;
    QSignalBlocker fromBlocker(fromCombo_);
    QSignalBlocker toBlocker(toCombo_);
    fromCombo_->clear();
    toCombo_->clear();
    for (Index i = 0; i < editor_.network().map().stationCount(); ++i) {
        const Station& station = editor_.network().map().station(i);
        const QString label = stationDisplay(i);
        const QString stationId = QString::fromUtf8(station.id.c_str());
        fromCombo_->addItem(label, stationId);
        toCombo_->addItem(label, stationId);
    }
    auto restore = [](QComboBox* combo, const QString& stationId) {
        int index = combo->findData(stationId);
        if (index < 0 && combo->count() > 0) {
            index = 0;
        }
        combo->setCurrentIndex(index);
    };
    restore(fromCombo_, keepFrom);
    restore(toCombo_, keepTo);
    if (toCombo_->currentIndex() == fromCombo_->currentIndex() && toCombo_->count() > 1) {
        toCombo_->setCurrentIndex((toCombo_->currentIndex() + 1) % toCombo_->count());
    }
    updatingPanels_ = false;
}

void MainWindow::syncLineSelection(int lineIndex) {
    if (updatingPanels_ || lineList_ == nullptr) {
        return;
    }
    updatingPanels_ = true;
    {
        QSignalBlocker blocker(lineList_);
        int row = -1;
        for (int i = 0; i < lineList_->count(); ++i) {
            if (lineList_->item(i)->data(Qt::UserRole).toInt() == lineIndex) {
                row = i;
                break;
            }
        }
        lineList_->setCurrentRow(row);
    }
    updatingPanels_ = false;
    updateActions();
}

int MainWindow::currentLineFromList() const {
    if (lineList_ == nullptr) {
        return -1;
    }
    const QListWidgetItem* item = lineList_->currentItem();
    if (item == nullptr) {
        return -1;
    }
    return item->data(Qt::UserRole).toInt();
}

void MainWindow::editLine(int lineIndex) {
    if (lineIndex < 0 ||
        lineIndex >= static_cast<int>(editor_.network().lines().size())) {
        return;
    }
    Line current = editor_.network().lines()[static_cast<std::size_t>(lineIndex)];
    while (true) {
        LineDialog dialog(current, editor_.network().map(), this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        Line updated = dialog.result();
        std::string error;
        if (editor_.replaceLine(lineIndex, updated, &error)) {
            break;
        }
        QMessageBox::warning(this, L("Invalid line"), localizeMessage(QString::fromUtf8(error.c_str())));
        current = updated;
    }
    view_->refresh();
    updateTitle();
    updateActions();
    refreshPanels();
    statusBar()->showMessage(L("Line updated"), 3000);
}

void MainWindow::deleteLine(int lineIndex) {
    std::string error;
    if (!editor_.removeLine(lineIndex, &error)) {
        QMessageBox::warning(this, L("Delete failed"), localizeMessage(QString::fromUtf8(error.c_str())));
        return;
    }
    view_->selectLine(-1);
    view_->refresh();
    updateTitle();
    updateActions();
    refreshPanels();
    statusBar()->showMessage(L("Line deleted"), 3000);
}

void MainWindow::editTransfers() {
    TransferDialog dialog(editor_.network(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    std::string error;
    if (!editor_.setTransferTimes(dialog.records(), &error)) {
        QMessageBox::warning(this, L("Transfer update failed"),
                             localizeMessage(QString::fromUtf8(error.c_str())));
        return;
    }
    view_->refresh();
    updateTitle();
    updateActions();
    refreshPanels();
    statusBar()->showMessage(L("Transfer times updated"), 3000);
}

void MainWindow::computeRoute() {
    if (routeOutput_ == nullptr || fromCombo_ == nullptr || toCombo_ == nullptr ||
        timeEdit_ == nullptr) {
        return;
    }
    if (!fromCombo_->currentData().isValid() || !toCombo_->currentData().isValid()) {
        routeOutput_->setPlainText(L("No stations available."));
        return;
    }
    const Index from = editor_.network().map().findStation(
        fromCombo_->currentData().toString().toUtf8().constData());
    const Index to = editor_.network().map().findStation(
        toCombo_->currentData().toString().toUtf8().constData());
    if (from == kInvalidIndex || to == kInvalidIndex) {
        routeOutput_->setPlainText(L("Selected station no longer exists."));
        return;
    }
    const double departTime = timeEdit_->time().msecsSinceStartOfDay() / 1000.0;

    Router router;
    const Journey journey = router.findRoute(editor_.network(), from, to, departTime);

    if (!journey.found) {
        routeOutput_->setPlainText(
            L("No route found for %1 at this time.").arg(stationName(editor_.network(), to)));
        return;
    }

    QString text = L("Depart %1   Arrive %2   Transfers %3\n\n")
                       .arg(formatClock(journey.departTime), formatClock(journey.arriveTime))
                       .arg(journey.transfers);
    for (std::size_t i = 0; i < journey.legs.size(); ++i) {
        const JourneyLeg& leg = journey.legs[i];
        const Line& line = editor_.network().lines()[static_cast<std::size_t>(leg.line)];
        text += L("%1. %2\n   %3 -> %4\n   %5 - %6\n")
                    .arg(static_cast<int>(i) + 1)
                    .arg(QString::fromUtf8(line.name().c_str()))
                    .arg(stationName(editor_.network(), leg.fromStation),
                         stationName(editor_.network(), leg.toStation))
                    .arg(formatClock(leg.boardTime), formatClock(leg.alightTime));
    }
    routeOutput_->setPlainText(text);
}

QString MainWindow::stationDisplay(int stationIndex) const {
    if (stationIndex < 0 ||
        stationIndex >= editor_.network().map().stationCount()) {
        return "?";
    }
    const Station& station = editor_.network().map().station(stationIndex);
    return QString::fromUtf8(station.name.c_str()) + " (" +
           QString::fromUtf8(station.id.c_str()) + ")";
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (firstShow_) {
        firstShow_ = false;
        QTimer::singleShot(0, this, [this] { view_->fitContent(); });
    }
}

}  // namespace uc
