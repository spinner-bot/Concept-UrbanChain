#include <QApplication>
#include <QStyleFactory>

#include "ui/main_window.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Concept UrbanChain");
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    uc::MainWindow window;
    window.show();
    return app.exec();
}
