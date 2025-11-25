#include "mainwindow.h"
#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 设置应用程序信息
    app.setApplicationName("BuffDetection");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("flairziv");
    app.setOrganizationDomain("github.com/flairziv");

    MainWindow w;
    w.show();

    return app.exec();
}
