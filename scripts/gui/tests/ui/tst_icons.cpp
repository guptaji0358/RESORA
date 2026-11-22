#include <QtTest>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QRegularExpression>

#include "ui/icons.h"
#include "ui/theme.h"

namespace {
bool hasColor(const QPixmap &pm, const QColor &c)
{
    const QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor p = img.pixelColor(x, y);
            if (p.alpha() > 200 && qAbs(p.red() - c.red()) < 4 && qAbs(p.green() - c.green()) < 4 && qAbs(p.blue() - c.blue()) < 4)
                return true;
        }
    return false;
}
}  // namespace

class IconsTest : public QObject
{
    Q_OBJECT
private slots:
    void everyFamilyMemberIsCompiledInAndRenders()
    {
        const QStringList names = QDir(Icons::resourcePath("folder").section('/', 0, -2)).entryList({"*.svg"});
        QVERIFY2(names.size() >= 20, "icon family missing from the Qt resources");
        for (const QString &file : names) {
            const QString name = QFileInfo(file).completeBaseName();
            QVERIFY2(Icons::exists(name), qPrintable(name));
            const QPixmap pm = Icons::pixmap(name, 24, Theme::paper(), Theme::ember());
            QVERIFY2(hasColor(pm, Theme::paper()) || hasColor(pm, Theme::ember()), qPrintable(name + " rendered empty"));
        }
    }

    void svgFilesOnlyUseTheTwoColourTokens()
    {
        const QRegularExpression hex("#[0-9A-Fa-f]{6}");
        for (const char *dir : {":/icons", ":/branding"}) {
            for (const QString &file : QDir(dir).entryList({"*.svg"})) {
                QFile f(QStringLiteral("%1/%2").arg(dir, file));
                QVERIFY(f.open(QIODevice::ReadOnly));
                auto it = hex.globalMatch(QString::fromUtf8(f.readAll()));
                while (it.hasNext()) {
                    const QString c = it.next().captured().toUpper();
                    QVERIFY2(c == "#EFE9DD" || c == "#FF5B2E", qPrintable(file + " uses " + c));
                }
            }
        }
    }

    void tintingFollowsRequestedColours()
    {
        const QColor main(10, 200, 30), accent(200, 10, 220);
        const QPixmap pm = Icons::pixmap("scan", 48, main, accent);
        QVERIFY(hasColor(pm, main));
        QVERIFY(hasColor(pm, accent));
        QVERIFY(!hasColor(pm, Theme::paper()));
    }

    void rendersSharpAtHighDpi()
    {
        const QPixmap pm = Icons::pixmap("folder", 24, Theme::paper(), Theme::ember(), 2.0);
        QCOMPARE(pm.size(), QSize(48, 48));
        QCOMPARE(pm.devicePixelRatio(), 2.0);
        const QPixmap viaIcon = Icons::icon("folder").pixmap(QSize(24, 24), 3.0);
        QCOMPARE(viaIcon.size(), QSize(72, 72));
    }

    void iconStatesDiffer()
    {
        const QIcon ic = Icons::icon("add");
        const QImage normal = ic.pixmap(32, QIcon::Normal).toImage();
        const QImage disabled = ic.pixmap(32, QIcon::Disabled).toImage();
        const QImage active = ic.pixmap(32, QIcon::Active).toImage();
        QVERIFY(normal != disabled);
        QVERIFY(normal != active);
        QVERIFY(hasColor(QPixmap::fromImage(disabled), Icons::dim()));
    }

    void missingAssetIsVisibleNotSilent()
    {
        QVERIFY(!Icons::exists("no-such-icon"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("missing icon asset 'no-such-icon'"));
        const QPixmap pm = Icons::pixmap("no-such-icon", 24, Theme::paper(), Theme::ember());
        QCOMPARE(pm.size(), QSize(24, 24));
        QVERIFY(hasColor(pm, Theme::alert()));
    }

    void brandMarkAndIllustrationLoad()
    {
        QVERIFY(Icons::exists("branding/mark"));
        QVERIFY(hasColor(Icons::artwork("branding/mark", QSize(64, 64)), Theme::ember()));
        const QPixmap art = Icons::artwork("illustrations/empty-results", QSize(200, 125));
        QCOMPARE(art.size(), QSize(200, 125));
        QVERIFY(hasColor(art, Theme::ember()));
    }
};

QTEST_MAIN(IconsTest)
#include "tst_icons.moc"
