#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>
#include <cmath>

#include "backend/comparison.h"
#include "widgets/aspectindicator.h"
#include "widgets/collapsiblesection.h"
#include "widgets/comparisontable.h"
#include "widgets/dimensiondiagram.h"
#include "widgets/filesizebars.h"
#include "widgets/insightsview.h"
#include "widgets/metadatacompare.h"
#include "widgets/resolutionchart.h"

namespace {
// bytes < 0 -> file_size null (missing); w/h <= 0 -> dimensions reported as 0.
QJsonObject props(const QString &name, int w, int h, qint64 bytes, bool exif = false, const QString &label = QString(), bool dpi = true)
{
    const double ar = h > 0 ? double(w) / h : 0;
    QJsonObject color{{"mode", "RGB"}, {"mode_name", "RGB"}, {"channels", 3}, {"bits_per_channel", 8}, {"has_alpha", false}, {"icc_profile", QJsonValue()}};
    return QJsonObject{{"path", "C:/x/" + name}, {"name", name}, {"status", "ok"}, {"error", QJsonValue()},
                       {"file_size", bytes < 0 ? QJsonValue() : QJsonValue(bytes)}, {"format", "PNG"}, {"warnings", QJsonArray()},
                       {"layout", w >= h ? "landscape" : "portrait"}, {"aspect_ratio", ar},
                       {"aspect_label", label.isEmpty() ? QString("other") : label}, {"megapixels", double(w) * h / 1e6},
                       {"resolution", "x"}, {"color", color},
                       {"dpi", dpi ? QJsonValue(QJsonObject{{"x", 72.0}, {"y", 72.0}, {"source", "file header"}}) : QJsonValue()},
                       {"exif", QJsonObject{{"available", exif}, {"fields", QJsonArray()}, {"gps_present", false}, {"orientation", QJsonValue()}}},
                       {"frames", QJsonValue()},
                       {"dimensions", QJsonObject{{"stored_width", w}, {"stored_height", h}, {"width", w}, {"height", h},
                                                  {"orientation_tag", QJsonValue()}, {"orientation_label", QJsonValue()}, {"exif_rotated", false}}}};
}

QString joined(const InsightMetrics &m)
{
    return m.verdict.join('\n');
}
}  // namespace

class TstInsights : public QObject
{
    Q_OBJECT
private slots:
    void differentResolutions()
    {
        const Comparison c = ComparisonModel::compare(props("a.png", 1000, 500, 2000, false, "2:1"), props("b.png", 2000, 1000, 8000, false, "2:1"));
        const InsightMetrics &m = c.metrics;
        QVERIFY(m.pixelsComparable);
        QCOMPARE(m.morePixels, Side::B);
        QVERIFY(qFuzzyCompare(m.pixelRatio, 4.0));
        QVERIFY(qFuzzyCompare(m.mpDifference, 1.5));
        QVERIFY(qFuzzyCompare(m.pixelPercent, 300.0));
        QVERIFY(joined(m).contains("Image B has 4.00× the pixel count of Image A"));
        QVERIFY(joined(m).contains("Image A occupies less storage"));
        QVERIFY(joined(m).contains("same aspect ratio (2:1)"));
        QVERIFY(!joined(m).contains("better quality"));  // never claims quality
        QVERIFY(joined(m).contains("do not by themselves say which image looks better"));
    }
    void identicalResolutionsAndSizes()
    {
        const Comparison c = ComparisonModel::compare(props("a.png", 800, 600, 10), props("b.png", 800, 600, 10));
        QCOMPARE(c.metrics.morePixels, Side::Tie);
        QCOMPARE(c.metrics.largerFile, Side::Tie);
        QVERIFY(joined(c.metrics).contains("same pixel count"));
        QVERIFY(joined(c.metrics).contains("same size"));
    }
    void zeroAndMissingValuesAreSafe()
    {
        // 0 bytes is a real value: bar empty, percentage undefined (no division by zero).
        Comparison c = ComparisonModel::compare(props("a.png", 10, 10, 0), props("b.png", 10, 10, 500));
        QVERIFY(c.metrics.sizesComparable);
        QCOMPARE(c.metrics.largerFile, Side::B);
        QVERIFY(!c.metrics.bytePercentValid);
        // Missing size on one side: not comparable, other side still drawn.
        c = ComparisonModel::compare(props("a.png", 10, 10, -1), props("b.png", 10, 10, 500));
        QVERIFY(!c.metrics.a.hasBytes && c.metrics.b.hasBytes);
        QVERIFY(!c.metrics.sizesComparable);
        QVERIFY(joined(c.metrics).contains("cannot be compared"));
        // Zero dimensions are invalid, not a tiny image.
        c = ComparisonModel::compare(props("a.png", 0, 0, 5), props("b.png", 10, 10, 5));
        QVERIFY(c.complete);
        QVERIFY(!c.metrics.pixelsComparable);
        QVERIFY(!c.metrics.a.hasSize);
        QVERIFY(joined(c.metrics).contains("Pixel counts cannot be compared"));
    }
    void aspectRelations()
    {
        AspectIndicator ind;
        ind.setMetrics(ComparisonModel::compare(props("a.png", 1920, 1080, 1, false, "16:9"), props("b.png", 1280, 720, 1, false, "16:9")).metrics);
        QCOMPARE(ind.relation(), AspectIndicator::Relation::Identical);
        ind.setMetrics(ComparisonModel::compare(props("a.png", 1920, 1080, 1, false, "16:9"), props("b.png", 1024, 768, 1, false, "4:3")).metrics);
        QCOMPARE(ind.relation(), AspectIndicator::Relation::Different);
        // 1.000 vs 1.015: below 2 % and labelled differently -> "nearly identical", not a difference alarm.
        ind.setMetrics(ComparisonModel::compare(props("a.png", 1000, 1000, 1, false, "1:1"), props("b.png", 1015, 1000, 1, false, "1.02:1")).metrics);
        QCOMPARE(ind.relation(), AspectIndicator::Relation::NearlyIdentical);
        QVERIFY(AspectIndicator::scalePosition(1.0) > 0.499 && AspectIndicator::scalePosition(1.0) < 0.501);
        QCOMPARE(AspectIndicator::scalePosition(100.0), 1.0);  // clamped, never off-scale
        QCOMPARE(AspectIndicator::scalePosition(0.001), 0.0);
    }
    void metadataTable()
    {
        const Comparison c = ComparisonModel::compare(props("a.png", 10, 10, 1, true, QString(), true), props("b.png", 10, 10, 1, false, QString(), false));
        QCOMPARE(c.metrics.meta.size(), 6);
        const auto find = [&](const QString &l) { for (const MetaRow &r : c.metrics.meta) if (r.label == l) return r; return MetaRow(); };
        QVERIFY(find("EXIF").differs);
        QVERIFY(find("EXIF").aAvailable && !find("EXIF").bAvailable);
        QCOMPARE(find("EXIF").b, PropertiesModel::unavailableText());
        QVERIFY(find("DPI / PPI").differs);
        QVERIFY(find("DPI / PPI").b != "0");
        QVERIFY(!find("Format").differs);
        QVERIFY(!find("Color mode").differs);
        MetadataCompare t;
        t.setMetrics(c.metrics);
        QCOMPARE(t.rowCount(), 6);
        QCOMPARE(t.differingRows(), c.metrics.meta.size() - 4);  // EXIF and DPI differ; the rest match
    }
    void dimensionDiagramKeepsProportions()
    {
        DimensionDiagram d;
        d.resize(700, 300);
        d.setMetrics(ComparisonModel::compare(props("a.png", 1920, 1080, 1), props("b.png", 1000, 1000, 1)).metrics);
        const auto l = d.layoutRects();
        QVERIFY(l.commonScale);
        QVERIFY(std::abs(l.a.width() / l.a.height() - 1920.0 / 1080.0) < 1e-6);
        QVERIFY(std::abs(l.b.width() / l.b.height() - 1.0) < 1e-6);
        QVERIFY(std::abs(l.a.width() / 1920.0 - l.b.width() / 1000.0) < 1e-6);  // one common scale
        QVERIFY(std::abs(l.a.bottom() - l.b.bottom()) < 1e-6);
        // Huge size gap: tiny rectangle would vanish, so scales are independent but proportions exact.
        d.setMetrics(ComparisonModel::compare(props("a.png", 8000, 6000, 1), props("b.png", 40, 30, 1)).metrics);
        const auto g = d.layoutRects();
        QVERIFY(!g.commonScale);
        QVERIFY(std::abs(g.b.width() / g.b.height() - 40.0 / 30.0) < 1e-6);
        QVERIFY(g.b.height() >= 16);
    }
    void chartsRenderAtSmallAndLargeSizes()
    {
        const Comparison full = ComparisonModel::compare(props("a-very-long-file-name-for-testing-elision.png", 1920, 1080, 123456, true, "16:9"),
                                                         props("b.png", 640, 480, 0, false, "4:3"));
        const Comparison broken = ComparisonModel::compare(props("a.png", 0, 0, -1), props("b.png", -5, 10, -1));
        for (const Comparison *c : {&full, &broken})
            for (const QSize sz : {QSize(220, 300), QSize(320, 300), QSize(900, 400), QSize(1600, 500)}) {
                ResolutionChart r; FileSizeBars f; DimensionDiagram d; AspectIndicator a;
                for (InsightCanvas *w : std::initializer_list<InsightCanvas *>{&r, &f, &d, &a}) {
                    w->setMetrics(c->metrics);
                    w->resize(sz.width(), qMax(w->minimumHeight(), sz.height()));
                    const QImage img = w->grab().toImage();  // must not crash or paint outside
                    QVERIFY(!img.isNull());
                    QVERIFY(!w->accessibleName().isEmpty());
                    QVERIFY(!w->accessibleDescription().isEmpty());
                }
            }
        ResolutionChart r;
        r.setMetrics(broken.metrics);
        QVERIFY(!r.showsData());
        QVERIFY(r.unavailableMessage().contains("Image A"));
        FileSizeBars f;
        f.setMetrics(full.metrics);
        QVERIFY(f.showsData());  // 0 bytes is drawn (empty bar), not hidden
    }
    void insightsViewAssemblyAndCollapse()
    {
        ComparisonTable t;
        t.resize(500, 600);
        QVERIFY(!t.insights());
        t.setComparison(ComparisonModel::compare(props("a.png", 1920, 1080, 100, false, "16:9"), props("b.png", 640, 480, 50, false, "4:3")));
        QVERIFY(t.insights());
        QVERIFY(t.insights()->verdictCount() >= 3);
        QVERIFY(!t.insights()->explanations()->isExpanded());  // secondary by default
        t.insights()->explanations()->setExpanded(true);
        QVERIFY(t.insights()->explanations()->isExpanded());
        QVERIFY(t.rowCount() > 0);  // the full property table is still there
        t.setMessage("x");
        QVERIFY(!t.insights());
    }
};

QTEST_MAIN(TstInsights)
#include "tst_insights.moc"
