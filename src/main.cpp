#include "gui/MainWindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("SmartCity Pro");
    QApplication::setOrganizationName("SmartCity Pro University Project");

    QFont applicationFont("Segoe UI");
    applicationFont.setPointSize(10);
    app.setFont(applicationFont);

    MainWindow window;
    window.show();
    return app.exec();
}

