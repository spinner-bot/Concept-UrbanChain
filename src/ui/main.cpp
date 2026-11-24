#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QStyleFactory>
#include <QTimer>
#include <QWidget>

#include "ui/language.h"
#include "ui/main_window.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("ConceptUrbanChain");
    QCoreApplication::setApplicationName("ConceptUrbanChain");
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    uc::initializeLanguage();

    QString mode;
    QString path;
    QString languageOverride;
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument == "--lang" && i + 1 < argc) {
            languageOverride = QString::fromLocal8Bit(argv[++i]);
        } else if (mode.isEmpty()) {
            mode = argument;
        } else if (path.isEmpty()) {
            path = argument;
        }
    }
    if (languageOverride == "zh") {
        uc::setLanguage(uc::Language::Chinese, false);
    } else if (languageOverride == "en") {
        uc::setLanguage(uc::Language::English, false);
    }

    uc::MainWindow window;
    window.show();

    // 开发自检入口：
    //   --screenshot <path>        截取窗口
    //   --screenshot-route <path>  选择 WEST -> SE 并查询，然后截图
    //   --screenshot-line <path>   打开线路属性对话框并截图
    //   --screenshot-transfers <path> 打开换乘时间对话框并截图
    if (mode == "--screenshot-route" && !path.isEmpty()) {
        QTimer::singleShot(600, &window, [&window, path] {
            auto* from = window.findChild<QComboBox*>("fromCombo");
            auto* to = window.findChild<QComboBox*>("toCombo");
            auto* button = window.findChild<QPushButton*>("routeButton");
            if (from != nullptr && to != nullptr && button != nullptr) {
                const int fromIndex = from->findData(QStringLiteral("WEST"));
                const int toIndex = to->findData(QStringLiteral("SE"));
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
