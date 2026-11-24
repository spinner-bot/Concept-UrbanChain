#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QStyleFactory>
#include <QTimer>
#include <QWidget>

#include "ui/main_window.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Concept UrbanChain");
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    uc::MainWindow window;
    window.show();

    // 开发自检入口：
    //   --screenshot <path>        截取窗口
    //   --screenshot-route <path>  选择 WEST -> SE 并查询，然后截图
    const QString mode = argc >= 2 ? QString::fromLocal8Bit(argv[1]) : QString();
    const QString path = argc >= 3 ? QString::fromLocal8Bit(argv[2]) : QString();
    if (mode == "--screenshot-route" && !path.isEmpty()) {
        QTimer::singleShot(600, &window, [&window, path] {
            auto* from = window.findChild<QComboBox*>("fromCombo");
            auto* to = window.findChild<QComboBox*>("toCombo");
            auto* button = window.findChild<QPushButton*>("routeButton");
            if (from != nullptr && to != nullptr && button != nullptr) {
                const int fromIndex = from->findData(1);
                const int toIndex = to->findData(6);
                if (fromIndex >= 0) {
                    from->setCurrentIndex(fromIndex);
                }
                if (toIndex >= 0) {
                    to->setCurrentIndex(toIndex);
                }
                button->click();
            }
            QTimer::singleShot(300, &window, [&window, path] {
                window.grab().save(path);
                QApplication::quit();
            });
        });
    } else if (mode == "--screenshot-line" && !path.isEmpty()) {
        QTimer::singleShot(600, &window, [&window] {
            auto* list = window.findChild<QListWidget*>("lineList");
            auto* edit = window.findChild<QPushButton*>("editLineButton");
            if (list != nullptr && edit != nullptr && list->count() > 0) {
                list->setCurrentRow(0);
                edit->click();
            }
        });
        QTimer::singleShot(1500, &window, [&window, path] {
            if (QWidget* modal = QApplication::activeModalWidget()) {
                modal->grab().save(path);
                modal->close();
            } else {
                window.grab().save(path);
            }
            QApplication::quit();
        });
    } else if (mode == "--screenshot-transfers" && !path.isEmpty()) {
        QTimer::singleShot(600, &window, [&window] {
            auto* action = window.findChild<QAction*>("transferAction");
            if (action != nullptr) {
                action->trigger();
            }
        });
        QTimer::singleShot(1500, &window, [&window, path] {
            if (QWidget* modal = QApplication::activeModalWidget()) {
                modal->grab().save(path);
                modal->close();
            } else {
                window.grab().save(path);
            }
            QApplication::quit();
        });
    } else if (mode == "--screenshot" && !path.isEmpty()) {
        QTimer::singleShot(800, &window, [&window, path] {
            window.grab().save(path);
            QApplication::quit();
        });
    }
    return app.exec();
}
