#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QGraphicsView>
#include <QInputDialog>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>
#include <QtTest/QtTest>

#include "ui/main_window.h"

class UiSmokeTest : public QObject {
    Q_OBJECT

private slots:
    void selectionAndRoute();
    void editingWorkflow();
};

void UiSmokeTest::selectionAndRoute() {
    uc::MainWindow window;
    window.resize(1280, 840);
    window.show();
    QTest::qWait(600);

    auto* view = window.findChild<QGraphicsView*>("mapView");
    auto* lineList = window.findChild<QListWidget*>("lineList");
    auto* editButton = window.findChild<QPushButton*>("editLineButton");
    auto* fromCombo = window.findChild<QComboBox*>("fromCombo");
    auto* toCombo = window.findChild<QComboBox*>("toCombo");
    auto* routeButton = window.findChild<QPushButton*>("routeButton");
    auto* routeOutput = window.findChild<QPlainTextEdit*>("routeOutput");

    QVERIFY(view != nullptr);
    QVERIFY(lineList != nullptr);
    QVERIFY(editButton != nullptr);
    QVERIFY(fromCombo != nullptr);
    QVERIFY(toCombo != nullptr);
    QVERIFY(routeButton != nullptr);
    QVERIFY(routeOutput != nullptr);

    QCOMPARE(lineList->count(), 2);

    // 点击红线中点，应选中第 1 条线路并启用编辑按钮。
    const QPoint redPoint = view->mapFromScene(QPointF(-0.5, 0.0));
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, redPoint);
    QCOMPARE(lineList->currentRow(), 0);
    QVERIFY(editButton->isEnabled());

    // 点击蓝线中点，应切换到第 2 条线路。
    const QPoint bluePoint = view->mapFromScene(QPointF(0.0, -0.5));
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, bluePoint);
    QCOMPARE(lineList->currentRow(), 1);

    // 查询 WEST -> SE，结果面板应出现行程信息。
    fromCombo->setCurrentIndex(fromCombo->findData(QStringLiteral("WEST")));
    toCombo->setCurrentIndex(toCombo->findData(QStringLiteral("SE")));
    routeButton->click();
    const QString text = routeOutput->toPlainText();
    QVERIFY2(text.contains("Arrive"), qPrintable(text));
    QVERIFY2(text.contains("Transfers"), qPrintable(text));
    QVERIFY2(text.contains("Red"), qPrintable(text));
    QVERIFY2(text.contains("Blue"), qPrintable(text));
}

void UiSmokeTest::editingWorkflow() {
    uc::MainWindow window;
    window.resize(1280, 840);
    window.show();
    QTest::qWait(600);

    auto* view = window.findChild<QGraphicsView*>("mapView");
    auto* lineList = window.findChild<QListWidget*>("lineList");
    auto* fromCombo = window.findChild<QComboBox*>("fromCombo");
    auto* addStation = window.findChild<QAction*>("addStationAction");
    auto* addEdge = window.findChild<QAction*>("addEdgeAction");
    auto* drawLine = window.findChild<QAction*>("drawLineAction");
    auto* deleteAction = window.findChild<QAction*>("deleteAction");
    auto* finishLine = window.findChild<QAction*>("finishLineAction");
    auto* undoAction = window.findChild<QAction*>("undoAction");
    auto* editButton = window.findChild<QPushButton*>("editLineButton");

    QVERIFY(view != nullptr);
    QVERIFY(lineList != nullptr);
    QVERIFY(fromCombo != nullptr);
    QVERIFY(addStation != nullptr);
    QVERIFY(addEdge != nullptr);
    QVERIFY(drawLine != nullptr);
    QVERIFY(deleteAction != nullptr);
    QVERIFY(finishLine != nullptr);
    QVERIFY(undoAction != nullptr);
    QVERIFY(editButton != nullptr);

    QCOMPARE(lineList->count(), 2);
    QCOMPARE(fromCombo->count(), 7);

    // 模态对话框自动处理：输入名称并确认。
    bool stationDialogHandled = false;
    QTimer stationTimer;
    QObject::connect(&stationTimer, &QTimer::timeout, [&] {
        if (auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) {
            dialog->setTextValue("Test Station");
            dialog->accept();
            stationDialogHandled = true;
            stationTimer.stop();
        }
    });
    stationTimer.start(50);
    addStation->trigger();
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view->mapFromScene(QPointF(0.5, 0.5)));
    stationTimer.stop();
    QVERIFY(stationDialogHandled);
    QCOMPARE(fromCombo->count(), 8);

    // 删除未使用的站点，再撤销恢复。
    deleteAction->trigger();
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view->mapFromScene(QPointF(0.5, 0.5)));
    QCOMPARE(fromCombo->count(), 7);
    undoAction->trigger();
    QCOMPARE(fromCombo->count(), 8);

    // 添加区间 WEST - SOUTH。
    addEdge->trigger();
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view->mapFromScene(QPointF(-1.0, 0.0)));
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view->mapFromScene(QPointF(0.0, -1.0)));
    QVERIFY(window.statusBar()->currentMessage().contains("Edge added"));

    // 绘制 WEST -> SOUTH 线路并结束。
    drawLine->trigger();
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view->mapFromScene(QPointF(-1.0, 0.0)));
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view->mapFromScene(QPointF(0.0, -1.0)));
    finishLine->trigger();
    QCOMPARE(lineList->count(), 3);

    // 删除新线路。
    deleteAction->trigger();
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view->mapFromScene(QPointF(-0.5, -0.5)));
    QCOMPARE(lineList->count(), 2);

    // 撤销删除。
    undoAction->trigger();
    QCOMPARE(lineList->count(), 3);

    // 打开线路属性对话框并确认，不应改变线路数量。
    lineList->setCurrentRow(0);
    bool lineDialogHandled = false;
    QTimer lineTimer;
    QObject::connect(&lineTimer, &QTimer::timeout, [&] {
        if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
            dialog->accept();
            lineDialogHandled = true;
            lineTimer.stop();
        }
    });
    lineTimer.start(50);
    editButton->click();
    lineTimer.stop();
    QVERIFY(lineDialogHandled);
    QCOMPARE(lineList->count(), 3);
}

QTEST_MAIN(UiSmokeTest)
#include "ui_smoke.moc"
