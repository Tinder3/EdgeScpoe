#include "MainWindow.h"

#include <QApplication>
#include <QMetaObject>
#include <QString>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName("EdgeScope");
    QApplication::setApplicationVersion("0.5.0");

    MainWindow window;
    window.show();
    if (application.arguments().contains("--smoke-test")) {
        QTimer::singleShot(0, &window, [&window] {
            QMetaObject::invokeMethod(&window, "ConnectToAgent");
        });
        QTimer::singleShot(2500, &application, &QApplication::quit);
    }
    return application.exec();
}
