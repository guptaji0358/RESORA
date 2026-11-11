#include <QApplication>
#include <QDebug>

#include "resora/resoraimport.h"

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
    // Remove extraction folders abandoned by a crashed session (conservative; see docs/RESORA_FORMAT.md).
    const resora::SweepReport swept = resora::sweepStaleCache();
    if (swept.removed > 0 || swept.failed > 0)
        qInfo() << "RESORA cache sweep: removed" << swept.removed << "stale folder(s), failed" << swept.failed;
    MainWindow window;
    window.show();
    return app.exec();
}
