#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "backend/comparison.h"
#include "backend/previewloader.h"
#include "widgets/comparisonviewer.h"
#include "widgets/comparisonworkspace.h"

namespace {
QJsonObject props(const QString &name, int w, int h, qint64 bytes, bool withDpi, bool withIcc = false)
{
    const double ar = double(w) / h;
    QJsonObject color{{"mode", "RGB"}, {"mode_name", "RGB"}, {"channels", 3}, {"bits_per_channel", 8}, {"has_alpha", false}};
    color["icc_profile"] = withIcc ? QJsonValue(QJsonObject{{"description", "sRGB"}, {"size", 3144}}) : QJsonValue();
    return QJsonObject{{"path", "C:/x/" + name}, {"name", name}, {"status", "ok"}, {"error", QJsonValue()},
                       {"file_size", bytes}, {"format", "PNG"}, {"warnings", QJsonArray()}, {"layout", w >= h ? "landscape" : "portrait"},
                       {"aspect_ratio", ar}, {"aspect_label", ar > 1.7 && ar < 1.8 ? "16:9" : "other"},
                       {"megapixels", double(w) * h / 1e6}, {"resolution", "x"}, {"color", color},
                       {"dpi", withDpi ? QJsonValue(QJsonObject{{"x", 72.0}, {"y", 72.0}, {"source", "file header"}}) : QJsonValue()},
                       {"exif", QJsonObject{{"available", false}, {"fields", QJsonArray()}, {"gps_present", false}, {"orientation", QJsonValue()}}},
                       {"frames", QJsonValue()},
                       {"dimensions", QJsonObject{{"stored_width", w}, {"stored_height", h}, {"width", w}, {"height", h},
                                                  {"orientation_tag", QJsonValue()}, {"orientation_label", QJsonValue()}, {"exif_rotated", false}}}};
}

const CompareRow *row(const Comparison &c, const QString &label)
{
    for (const CompareRow &r : c.rows)
        if (r.label == label)
            return &r;
    return nullptr;
}

QString makePng(const QTemporaryDir &d, const QString &name, int w, int h)
{
    QImage img(w, h, QImage::Format_RGB32);
    img.fill(Qt::red);
    const QString p = d.filePath(name);
    img.save(p);
    return p;
}
}  // namespace

class TstComparison : public QObject
{
    Q_OBJECT
private slots:
    void differentDimensionsAndAspect()
    {
        const Comparison c = ComparisonModel::compare(props("a.png", 1920, 1080, 1000, true), props("b.png", 1000, 1000, 4000, true));
        QVERIFY(c.complete);
        QVERIFY(row(c, "Displayed size")->differs);
        QVERIFY(row(c, "Aspect ratio")->differs);
        QVERIFY(row(c, "File size")->differs);
        QVERIFY(!row(c, "Format")->differs);
        QVERIFY(c.insights.join(' ').contains("a.png has the greater pixel count"));
        QVERIFY(c.insights.join(' ').contains("b.png has the larger file"));
        QVERIFY(c.insights.join(' ').contains("-920"));  // width difference, right minus left
        QVERIFY(c.insights.last().contains("not the same as better quality"));
    }
    void equalImagesHaveNoDifferences()
    {
        const Comparison c = ComparisonModel::compare(props("a.png", 800, 600, 10, false), props("b.png", 800, 600, 10, false));
        QVERIFY(row(c, "Displayed size") && !row(c, "Displayed size")->differs);
        QVERIFY(c.insights.join(' ').contains("same pixel count"));
    }
    void missingMetadataIsNotZeroNorADifference()
    {
        const Comparison both = ComparisonModel::compare(props("a.png", 10, 10, 5, false), props("b.png", 10, 10, 5, false));
        const CompareRow *dpi = row(both, "DPI / PPI");
        QVERIFY(dpi);
        QVERIFY(!dpi->leftAvailable && !dpi->rightAvailable);
        QVERIFY(!dpi->differs);
        QCOMPARE(dpi->left, PropertiesModel::unavailableText());
        const Comparison one = ComparisonModel::compare(props("a.png", 10, 10, 5, true, true), props("b.png", 10, 10, 5, false));
        QVERIFY(row(one, "DPI / PPI")->differs);
        QVERIFY(row(one, "Color profile")->differs);
        QVERIFY(!row(one, "DPI / PPI")->rightAvailable);
        QVERIFY(row(one, "DPI / PPI")->right != "0");
    }
    void unreadableSideGivesNoComparison()
    {
        QJsonObject bad{{"path", "x.png"}, {"name", "x.png"}, {"status", "corrupted"}, {"error", "truncated"}};
        const Comparison c = ComparisonModel::compare(props("a.png", 10, 10, 5, false), bad);
        QVERIFY(!c.complete);
        QVERIFY(c.rows.isEmpty());
        QVERIFY(c.insights.isEmpty());
    }
    void percent()
    {
        QCOMPARE(ComparisonModel::percentChange(100, 125), QString("+25.0%"));
        QCOMPARE(ComparisonModel::percentChange(100, 50), QString("-50.0%"));
        QVERIFY(ComparisonModel::percentChange(0, 5).isEmpty());
    }

    void previewLoaderValidDownscaledAndBad()
    {
        QTemporaryDir dir;
        const QString big = makePng(dir, "big.png", 800, 400);
        PreviewResult r = PreviewLoader::load(big);
        QVERIFY(r.ok());
        QCOMPARE(r.fullSize, QSize(800, 400));
        QVERIFY(!r.downscaled);
        r = PreviewLoader::load(big, 100);
        QVERIFY(r.ok());
        QVERIFY(r.downscaled);
        QCOMPARE(r.fullSize, QSize(800, 400));  // original size still reported
        QCOMPARE(r.image.width(), 100);
        QCOMPARE(r.image.height(), 50);

        QFile junk(dir.filePath("junk.png"));
        QVERIFY(junk.open(QIODevice::WriteOnly));
        junk.write("this is not a png");
        junk.close();
        QVERIFY(!PreviewLoader::load(junk.fileName()).ok());
        QVERIFY(!PreviewLoader::load(junk.fileName()).error.isEmpty());
        QVERIFY(!PreviewLoader::load(dir.filePath("missing.png")).ok());
    }

    void paneFitZoomPanReset()
    {
        ImagePane pane;
        pane.resize(424, 224);
        pane.setPreview(QImage(800, 400, QImage::Format_RGB32), QSize(800, 400));
        QVERIFY(pane.isFit());
        QVERIFY(qFuzzyCompare(pane.zoom(), 0.5));  // (424-24)/800, aspect preserved by one scale
        pane.zoomBy(2.0, QPointF(212, 112));
        QVERIFY(!pane.isFit());
        QVERIFY(qFuzzyCompare(pane.zoom(), 1.0));
        pane.panBy(QPointF(30, 10));
        QCOMPARE(pane.pan(), QPointF(30, 10));
        pane.actualSize();
        QVERIFY(qFuzzyCompare(pane.zoom(), 1.0));
        QCOMPARE(pane.pan(), QPointF());
        pane.fit();
        QVERIFY(pane.isFit());
        QVERIFY(qFuzzyCompare(pane.zoom(), 0.5));
    }

    void synchronizedAndIndependentZoom()
    {
        ComparisonViewer v;
        v.resize(900, 300);
        v.left()->setPreview(QImage(800, 400, QImage::Format_RGB32), QSize(800, 400));
        v.right()->setPreview(QImage(400, 800, QImage::Format_RGB32), QSize(400, 800));
        const double r0 = v.right()->zoom();
        v.left()->zoomBy(2.0, QPointF(10, 10));
        QVERIFY(qFuzzyCompare(v.right()->zoom(), r0 * 2.0));  // synced, ratio kept
        const QPointF rightBefore = v.right()->pan();
        v.left()->panBy(QPointF(15, 5));
        QCOMPARE(v.right()->pan() - rightBefore, QPointF(15, 5));  // same screen distance

        v.setSyncZoom(false);
        v.setSyncPan(false);
        const double r1 = v.right()->zoom();
        const QPointF p1 = v.right()->pan();
        v.left()->zoomBy(1.5, QPointF(10, 10));
        v.left()->panBy(QPointF(5, 5));
        QVERIFY(qFuzzyCompare(v.right()->zoom(), r1));
        QCOMPARE(v.right()->pan(), p1);

        v.fitBoth();
        QVERIFY(v.left()->isFit() && v.right()->isFit());
        v.actualSizeBoth();
        QVERIFY(qFuzzyCompare(v.left()->zoom(), 1.0) && qFuzzyCompare(v.right()->zoom(), 1.0));
        v.resetView();
        QVERIFY(v.left()->isFit() && v.right()->isFit());
    }

    void workspaceSelectionReplaceSwapClear()
    {
        QTemporaryDir dir;
        const QString a = QDir::toNativeSeparators(makePng(dir, "a.png", 64, 32));
        const QString b = QDir::toNativeSeparators(makePng(dir, "b.png", 20, 40));
        const QString c = QDir::toNativeSeparators(makePng(dir, "c.png", 10, 10));
        QFile txt(dir.filePath("notes.txt"));
        QVERIFY(txt.open(QIODevice::WriteOnly));
        txt.close();
        QFile junk(dir.filePath("junk.png"));
        QVERIFY(junk.open(QIODevice::WriteOnly));
        junk.write("garbage");
        junk.close();

        ComparisonWorkspace w;
        w.resize(1100, 800);
        QVERIFY(w.setImage(ComparisonWorkspace::Left, a).isEmpty());
        QVERIFY(w.setImage(ComparisonWorkspace::Right, b).isEmpty());
        QVERIFY(w.hasBoth());
        QTRY_VERIFY_WITH_TIMEOUT(w.viewer()->left()->hasImage() && w.viewer()->right()->hasImage(), 10000);
        QCOMPARE(w.viewer()->left()->fullSize(), QSize(64, 32));
        QCOMPARE(w.viewer()->right()->fullSize(), QSize(20, 40));  // different aspect, not stretched

        // Rejected files leave the slot untouched.
        QVERIFY(!w.setImage(ComparisonWorkspace::Left, txt.fileName()).isEmpty());
        QVERIFY(!w.setImage(ComparisonWorkspace::Left, dir.filePath("nope.png")).isEmpty());
        QCOMPARE(w.path(ComparisonWorkspace::Left), a);

        w.swapImages();
        QCOMPARE(w.path(ComparisonWorkspace::Left), b);
        QCOMPARE(w.path(ComparisonWorkspace::Right), a);
        QTRY_COMPARE_WITH_TIMEOUT(w.viewer()->left()->fullSize(), QSize(20, 40), 5000);

        QVERIFY(w.setImage(ComparisonWorkspace::Right, c).isEmpty());  // replace one side only
        QCOMPARE(w.path(ComparisonWorkspace::Left), b);
        QTRY_COMPARE_WITH_TIMEOUT(w.viewer()->right()->fullSize(), QSize(10, 10), 5000);

        // Corrupted file: accepted, reported in the pane, never crashes.
        QVERIFY(w.setImage(ComparisonWorkspace::Right, QDir::toNativeSeparators(junk.fileName())).isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!w.viewer()->right()->hasImage() && !w.isBusy(), 20000);

        w.clearImage(ComparisonWorkspace::Left);
        QVERIFY(w.path(ComparisonWorkspace::Left).isEmpty());
        QVERIFY(!w.viewer()->left()->hasImage());
        QVERIFY(w.setImage(ComparisonWorkspace::Left, c).isEmpty());  // select another
        QTRY_VERIFY_WITH_TIMEOUT(!w.isBusy(), 20000);
    }
};

QTEST_MAIN(TstComparison)
#include "tst_comparison.moc"
