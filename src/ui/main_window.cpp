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
    newAction_ = new QAction(tr("&New"), this);
    newAction_->setShortcut(QKeySequence::New);
    connect(newAction_, &QAction::triggered, this, [this] { newDocument(); });

    openAction_ = new QAction(tr("&Open..."), this);
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, this, [this] { openDocument(); });

    saveAction_ = new QAction(tr("&Save"), this);
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, [this] { saveDocument(); });

    saveAsAction_ = new QAction(tr("Save &As..."), this);
    saveAsAction_->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction_, &QAction::triggered, this, [this] { saveDocumentAs(); });

    exitAction_ = new QAction(tr("E&xit"), this);
    exitAction_->setShortcut(QKeySequence::Quit);
    connect(exitAction_, &QAction::triggered, this, [this] { close(); });

    undoAction_ = new QAction(tr("&Undo"), this);
    undoAction_->setObjectName("undoAction");
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, this, [this] {
        if (editor_.undo()) {
            view_->refresh();
            updateTitle();
            updateActions();
            refreshPanels();
            statusBar()->showMessage(tr("Undo"), 2000);
        }
    });

    redoAction_ = new QAction(tr("&Redo"), this);
    redoAction_->setShortcut(QKeySequence::Redo);
    connect(redoAction_, &QAction::triggered, this, [this] {
        if (editor_.redo()) {
            view_->refresh();
            updateTitle();
            updateActions();
            refreshPanels();
            statusBar()->showMessage(tr("Redo"), 2000);
        }
    });

    selectAction_ = new QAction(tr("&Select / Move"), this);
    selectAction_->setCheckable(true);
    selectAction_->setChecked(true);
    connect(selectAction_, &QAction::triggered, this, [this] { setMode(EditMode::Select); });

    addStationAction_ = new QAction(tr("Add &Station"), this);
    addStationAction_->setObjectName("addStationAction");
    addStationAction_->setCheckable(true);
    connect(addStationAction_, &QAction::triggered, this, [this] { setMode(EditMode::AddStation); });

    addEdgeAction_ = new QAction(tr("Add &Edge"), this);
    addEdgeAction_->setObjectName("addEdgeAction");
    addEdgeAction_->setCheckable(true);
    connect(addEdgeAction_, &QAction::triggered, this, [this] { setMode(EditMode::AddEdge); });

    drawLineAction_ = new QAction(tr("&Draw Line"), this);
    drawLineAction_->setObjectName("drawLineAction");
    drawLineAction_->setCheckable(true);
    connect(drawLineAction_, &QAction::triggered, this, [this] { setMode(EditMode::DrawLine); });

    deleteAction_ = new QAction(tr("&Delete"), this);
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

    finishLineAction_ = new QAction(tr("&Finish Line"), this);
    finishLineAction_->setObjectName("finishLineAction");
    finishLineAction_->setShortcut(QKeySequence(Qt::Key_Return));
    connect(finishLineAction_, &QAction::triggered, this, [this] {
        view_->finishLine();
        updateTitle();
        updateActions();
        refreshPanels();
    });

    removeLastStopAction_ = new QAction(tr("Remove Last &Stop"), this);
    removeLastStopAction_->setShortcut(QKeySequence(Qt::Key_Backspace));
    connect(removeLastStopAction_, &QAction::triggered, this, [this] {
        view_->removeLastStop();
        updateTitle();
        updateActions();
        refreshPanels();
    });

    transferAction_ = new QAction(tr("Transfer &Times..."), this);
    transferAction_->setObjectName("transferAction");
    connect(transferAction_, &QAction::triggered, this, [this] { editTransfers(); });

    fitAction_ = new QAction(tr("&Fit Content"), this);
    fitAction_->setShortcut(QKeySequence(Qt::Key_F));
    connect(fitAction_, &QAction::triggered, this, [this] { view_->fitContent(); });

    aboutAction_ = new QAction(tr("&About"), this);
    connect(aboutAction_, &QAction::triggered, this, [this] {
        QMessageBox::about(this, tr("About Concept UrbanChain"),
                           tr("Concept UrbanChain v0.3\n"
                              "A data-structure course project for metro network editing and "
                              "time-dependent routing."));
    });
}

void MainWindow::createMenus() {
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAction_);
    fileMenu->addAction(openAction_);
    fileMenu->addAction(saveAction_);
    fileMenu->addAction(saveAsAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction_);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(undoAction_);
    editMenu->addAction(redoAction_);

    QMenu* modeMenu = menuBar()->addMenu(tr("&Mode"));
    modeMenu->addAction(selectAction_);
    modeMenu->addAction(addStationAction_);
    modeMenu->addAction(addEdgeAction_);
    modeMenu->addAction(drawLineAction_);
    modeMenu->addAction(deleteAction_);

    QMenu* lineMenu = menuBar()->addMenu(tr("&Line"));
    lineMenu->addAction(finishLineAction_);
    lineMenu->addAction(removeLastStopAction_);
    lineMenu->addSeparator();
    lineMenu->addAction(transferAction_);

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(fitAction_);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAction_);
}

void MainWindow::createToolBar() {
    QToolBar* toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addAction(openAction_);
    toolbar->addAction(saveAction_);
    toolbar->addSeparator();
    toolbar->addAction(undoAction_);
    toolbar->addAction(redoAction_);
    toolbar->addSeparator();
    toolbar->addAction(selectAction_);
    toolbar->addAction(addStationAction_);
    toolbar->addAction(addEdgeAction_);
    toolbar->addAction(drawLineAction_);
    toolbar->addAction(deleteAction_);
    toolbar->addSeparator();
    toolbar->addAction(finishLineAction_);
}

void MainWindow::createStatusBar() {
    statusBar()->showMessage(tr("Ready"));
    modeLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(modeLabel_);
}

void MainWindow::createSidePanel() {
    auto* dock = new QDockWidget(tr("Network"), this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* panel = new QWidget(dock);
    panel->setMinimumWidth(320);
    auto* layout = new QVBoxLayout(panel);

    auto* linesGroup = new QGroupBox(tr("Lines"), panel);
    auto* linesLayout = new QVBoxLayout(linesGroup);
    lineList_ = new QListWidget(linesGroup);
    lineList_->setObjectName("lineList");
    lineList_->setMinimumHeight(150);
    linesLayout->addWidget(lineList_);
    auto* lineButtons = new QHBoxLayout();
    editLineButton_ = new QPushButton(tr("Edit..."), linesGroup);
    editLineButton_->setObjectName("editLineButton");
    deleteLineButton_ = new QPushButton(tr("Delete"), linesGroup);
    transferButton_ = new QPushButton(tr("Transfers..."), linesGroup);
    transferButton_->setObjectName("transferButton");
    lineButtons->addWidget(editLineButton_);
    lineButtons->addWidget(deleteLineButton_);
    lineButtons->addWidget(transferButton_);
    linesLayout->addLayout(lineButtons);
    layout->addWidget(linesGroup);

    auto* routeGroup = new QGroupBox(tr("Route"), panel);
    auto* routeLayout = new QVBoxLayout(routeGroup);
    auto* routeForm = new QFormLayout();
    fromCombo_ = new QComboBox(routeGroup);
    fromCombo_->setObjectName("fromCombo");
    toCombo_ = new QComboBox(routeGroup);
    toCombo_->setObjectName("toCombo");
    timeEdit_ = new QTimeEdit(QTime(8, 0), routeGroup);
    timeEdit_->setObjectName("timeEdit");
    timeEdit_->setDisplayFormat("HH:mm");
    routeForm->addRow(tr("From"), fromCombo_);
    routeForm->addRow(tr("To"), toCombo_);
    routeForm->addRow(tr("Depart"), timeEdit_);
    routeLayout->addLayout(routeForm);
    routeButton_ = new QPushButton(tr("Find Route"), routeGroup);
    routeButton_->setObjectName("routeButton");
    routeLayout->addWidget(routeButton_);
    routeOutput_ = new QPlainTextEdit(routeGroup);
    routeOutput_->setObjectName("routeOutput");
    routeOutput_->setReadOnly(true);
    routeOutput_->setMinimumHeight(180);
    routeOutput_->setPlaceholderText(tr("Route details appear here"));
    routeLayout->addWidget(routeOutput_, 1);
    layout->addWidget(routeGroup, 1);

    dock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, dock);

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
            statusBar()->showMessage(tr("Loaded demo network"), 3000);
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
    statusBar()->showMessage(tr("New document"), 3000);
}

void MainWindow::openDocument() {
    if (!maybeSave()) {
        return;
    }
    const QString mapPath = QFileDialog::getOpenFileName(
        this, tr("Open map"), QString(), tr("UrbanChain map (*.cuc3m);;All files (*)"));
    if (mapPath.isEmpty()) {
        return;
    }
    const QString linePath = QFileDialog::getOpenFileName(
        this, tr("Open line file"), QString(), tr("UrbanChain lines (*.cuc3l);;All files (*)"));
    if (linePath.isEmpty()) {
        return;
    }

    std::string error;
    if (!editor_.load(mapPath.toUtf8().constData(), linePath.toUtf8().constData(), &error)) {
        QMessageBox::critical(this, tr("Open failed"), QString::fromUtf8(error.c_str()));
        return;
    }
    mapPath_ = mapPath;
    linePath_ = linePath;
    view_->refresh();
    view_->fitContent();
    updateTitle();
    updateActions();
    refreshPanels();
    statusBar()->showMessage(tr("Loaded"), 3000);
}

bool MainWindow::saveDocument() {
    if (mapPath_.isEmpty() || linePath_.isEmpty()) {
        return saveDocumentAs();
    }
    return saveTo(mapPath_, linePath_);
}

bool MainWindow::saveDocumentAs() {
    const QString mapPath = QFileDialog::getSaveFileName(
        this, tr("Save map"), mapPath_.isEmpty() ? QString("map.cuc3m") : mapPath_,
        tr("UrbanChain map (*.cuc3m);;All files (*)"));
    if (mapPath.isEmpty()) {
        return false;
    }
    const QString linePath = QFileDialog::getSaveFileName(
        this, tr("Save line file"), linePath_.isEmpty() ? QString("lines.cuc3l") : linePath_,
        tr("UrbanChain lines (*.cuc3l);;All files (*)"));
    if (linePath.isEmpty()) {
        return false;
    }
    return saveTo(mapPath, linePath);
}

bool MainWindow::saveTo(const QString& mapPath, const QString& linePath) {
    std::string error;
    if (!editor_.save(mapPath.toUtf8().constData(), linePath.toUtf8().constData(), &error)) {
        QMessageBox::critical(this, tr("Save failed"), QString::fromUtf8(error.c_str()));
        return false;
    }
    mapPath_ = mapPath;
    linePath_ = linePath;
    updateTitle();
    updateActions();
    statusBar()->showMessage(tr("Saved"), 3000);
    return true;
}

bool MainWindow::maybeSave() {
    if (!editor_.isDirty()) {
        return true;
    }
    const QMessageBox::StandardButton answer = QMessageBox::warning(
        this, tr("Unsaved changes"), tr("Save changes before continuing?"),
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

void MainWindow::updateTitle() {
    const QString name = mapPath_.isEmpty() ? tr("Untitled") : QFileInfo(mapPath_).fileName();
    const QString suffix = editor_.isDirty() ? QString(" *") : QString();
    setWindowTitle(QString("Concept UrbanChain v0.3 - ") + name + suffix);
}

void MainWindow::updateActions() {
    undoAction_->setEnabled(editor_.canUndo());
    redoAction_->setEnabled(editor_.canRedo());

    QString mode = "Select";
    if (addStationAction_->isChecked()) {
        mode = "Add station";
    } else if (addEdgeAction_->isChecked()) {
        mode = "Add edge";
    } else if (drawLineAction_->isChecked()) {
        mode = "Draw line";
    } else if (deleteAction_->isChecked()) {
        mode = "Delete";
    }
    if (modeLabel_ != nullptr) {
        modeLabel_->setText(QString("%1 | %2 stations | %3 lines")
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
                QString("%1 (%2 stops)")
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
        QMessageBox::warning(this, tr("Invalid line"), QString::fromUtf8(error.c_str()));
        current = updated;
    }
    view_->refresh();
    updateTitle();
    updateActions();
    refreshPanels();
    statusBar()->showMessage(tr("Line updated"), 3000);
}

void MainWindow::deleteLine(int lineIndex) {
    std::string error;
    if (!editor_.removeLine(lineIndex, &error)) {
        QMessageBox::warning(this, tr("Delete failed"), QString::fromUtf8(error.c_str()));
        return;
    }
    view_->selectLine(-1);
    view_->refresh();
    updateTitle();
    updateActions();
    refreshPanels();
    statusBar()->showMessage(tr("Line deleted"), 3000);
}

void MainWindow::editTransfers() {
    TransferDialog dialog(editor_.network(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    std::string error;
    if (!editor_.setTransferTimes(dialog.records(), &error)) {
        QMessageBox::warning(this, tr("Transfer update failed"),
                             QString::fromUtf8(error.c_str()));
        return;
    }
    view_->refresh();
    updateTitle();
    updateActions();
    refreshPanels();
    statusBar()->showMessage(tr("Transfer times updated"), 3000);
}

void MainWindow::computeRoute() {
    if (routeOutput_ == nullptr || fromCombo_ == nullptr || toCombo_ == nullptr ||
        timeEdit_ == nullptr) {
        return;
    }
    if (!fromCombo_->currentData().isValid() || !toCombo_->currentData().isValid()) {
        routeOutput_->setPlainText(tr("No stations available."));
        return;
    }
    const Index from = editor_.network().map().findStation(
        fromCombo_->currentData().toString().toUtf8().constData());
    const Index to = editor_.network().map().findStation(
        toCombo_->currentData().toString().toUtf8().constData());
    if (from == kInvalidIndex || to == kInvalidIndex) {
        routeOutput_->setPlainText(tr("Selected station no longer exists."));
        return;
    }
    const double departTime = timeEdit_->time().msecsSinceStartOfDay() / 1000.0;

    Router router;
    const Journey journey = router.findRoute(editor_.network(), from, to, departTime);

    if (!journey.found) {
        routeOutput_->setPlainText(
            tr("No route found for %1 at this time.").arg(stationName(editor_.network(), to)));
        return;
    }

    QString text = tr("Depart %1   Arrive %2   Transfers %3\n\n")
                       .arg(formatClock(journey.departTime), formatClock(journey.arriveTime))
                       .arg(journey.transfers);
    for (std::size_t i = 0; i < journey.legs.size(); ++i) {
        const JourneyLeg& leg = journey.legs[i];
        const Line& line = editor_.network().lines()[static_cast<std::size_t>(leg.line)];
        text += QString("%1. %2\n   %3 -> %4\n   %5 - %6\n")
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
