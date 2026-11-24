#include "ui/main_window.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

namespace uc {

MainWindow::MainWindow() {
    view_ = new MapView(editor_, this);
    setCentralWidget(view_);
    resize(1200, 800);

    createActions();
    createMenus();
    createToolBar();
    createStatusBar();
    connectCallbacks();

    updateTitle();
    updateActions();
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
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, this, [this] {
        if (editor_.undo()) {
            view_->refresh();
            updateTitle();
            updateActions();
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
            statusBar()->showMessage(tr("Redo"), 2000);
        }
    });

    selectAction_ = new QAction(tr("&Select / Move"), this);
    selectAction_->setCheckable(true);
    selectAction_->setChecked(true);
    connect(selectAction_, &QAction::triggered, this, [this] { setMode(EditMode::Select); });

    addStationAction_ = new QAction(tr("Add &Station"), this);
    addStationAction_->setCheckable(true);
    connect(addStationAction_, &QAction::triggered, this, [this] { setMode(EditMode::AddStation); });

    addEdgeAction_ = new QAction(tr("Add &Edge"), this);
    addEdgeAction_->setCheckable(true);
    connect(addEdgeAction_, &QAction::triggered, this, [this] { setMode(EditMode::AddEdge); });

    drawLineAction_ = new QAction(tr("&Draw Line"), this);
    drawLineAction_->setCheckable(true);
    connect(drawLineAction_, &QAction::triggered, this, [this] { setMode(EditMode::DrawLine); });

    deleteLineAction_ = new QAction(tr("&Delete Line"), this);
    deleteLineAction_->setCheckable(true);
    connect(deleteLineAction_, &QAction::triggered, this, [this] { setMode(EditMode::DeleteLine); });

    auto* modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addAction(selectAction_);
    modeGroup->addAction(addStationAction_);
    modeGroup->addAction(addEdgeAction_);
    modeGroup->addAction(drawLineAction_);
    modeGroup->addAction(deleteLineAction_);

    finishLineAction_ = new QAction(tr("&Finish Line"), this);
    finishLineAction_->setShortcut(QKeySequence(Qt::Key_Return));
    connect(finishLineAction_, &QAction::triggered, this, [this] {
        view_->finishLine();
        updateTitle();
        updateActions();
    });

    removeLastStopAction_ = new QAction(tr("Remove Last &Stop"), this);
    removeLastStopAction_->setShortcut(QKeySequence(Qt::Key_Backspace));
    connect(removeLastStopAction_, &QAction::triggered, this, [this] {
        view_->removeLastStop();
        updateTitle();
        updateActions();
    });

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
    modeMenu->addAction(deleteLineAction_);

    QMenu* lineMenu = menuBar()->addMenu(tr("&Line"));
    lineMenu->addAction(finishLineAction_);
    lineMenu->addAction(removeLastStopAction_);

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
    toolbar->addAction(deleteLineAction_);
    toolbar->addSeparator();
    toolbar->addAction(finishLineAction_);
}

void MainWindow::createStatusBar() {
    statusBar()->showMessage(tr("Ready"));
    modeLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(modeLabel_);
}

void MainWindow::connectCallbacks() {
    view_->setStatusCallback([this](const QString& message) {
        statusBar()->showMessage(message, 4000);
    });
    view_->setChangeCallback([this] {
        updateTitle();
        updateActions();
    });
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
        QMessageBox::critical(this, tr("Open failed"),
                              QString::fromUtf8(error.c_str()));
        return;
    }
    mapPath_ = mapPath;
    linePath_ = linePath;
    view_->refresh();
    view_->fitContent();
    updateTitle();
    updateActions();
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
    } else if (mode == EditMode::DeleteLine) {
        deleteLineAction_->setChecked(true);
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
    } else if (deleteLineAction_->isChecked()) {
        mode = "Delete line";
    }
    if (modeLabel_ != nullptr) {
        modeLabel_->setText(QString("%1 | %2 stations | %3 lines")
                                .arg(mode)
                                .arg(editor_.network().map().stationCount())
                                .arg(static_cast<int>(editor_.network().lines().size())));
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

}  // namespace uc
