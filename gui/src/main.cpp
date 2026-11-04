#include <QApplication>

#include "mainwindow.h"
#include "ui/brand.h"
#include "ui/theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Theme::applyPalette(app);
    app.setApplicationName(Brand::name());
    app.setApplicationDisplayName(Brand::name());
    app.setApplicationVersion(Brand::version());
    app.setWindowIcon(Brand::icon());
    MainWindow window;
    window.show();
    return app.exec();
}
