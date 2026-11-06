#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

#include "backend/analyticsmodel.h"
#include "backend/scanrequest.h"
#include "widgets/analyticspanel.h"
#include "widgets/metriccard.h"
#include "widgets/phaserail.h"
#include "widgets/pixelspan.h"
#include "widgets/sharechart.h"

namespace {
// What the engine returns (shape documented in docs/ANALYTICS.md): 3 analyzed + 2 failed.
const char *kResult = R"JSON({"source":"folder","folder":"C:/pics","recursive":true,"duplicates_ignored":0,"analytics":{
 "denominator":3,
 "collection":{"total":5,"analyzed":3,"failed":2,"failed_by_status":{"corrupted":1,"invalid_metadata":1},"duplicates_ignored":0,
  "total_bytes":9000000000,"size_known":3,"size_unknown":0,"failed_bytes":10,"average_bytes":3000000000.0,
  "largest":{"path":"C:/pics/big.png","name":"big.png","width":3840,"height":2160,"megapixels":8.294,"file_size":6000000000,"format":"PNG","resolution":"UHD 4K"},
  "smallest":{"path":"C:/pics/s.jpg","name":"s.jpg","width":100,"height":100,"megapixels":0.01,"file_size":1000000000,"format":"JPEG","resolution":"Custom (below HD)"}},
 "resolution":{"total_pixels":8314400,"total_megapixels":8.314,"average_width":1980.0,"average_height":1420.0,"average_megapixels":2.771,
  "highest":{"path":"C:/pics/big.png","name":"big.png","width":3840,"height":2160,"megapixels":8.294,"file_size":6000000000,"format":"PNG","resolution":"UHD 4K","tied":1},
  "lowest":{"path":"C:/pics/s.jpg","name":"s.jpg","width":100,"height":100,"megapixels":0.01,"file_size":1000000000,"format":"JPEG","resolution":"x","tied":1},
  "most_common":{"width":3840,"height":2160,"count":1,"percent":33.33,"tied":3}},
 "categories":[{"name":"HD","count":0,"percent":0.0},{"name":"Full HD","count":1,"percent":33.33},{"name":"UHD 4K","count":1,"percent":33.33},{"name":"Below HD / Other","count":1,"percent":33.33}],
 "orientation":[{"name":"landscape","count":2,"percent":66.67},{"name":"portrait","count":0,"percent":0.0},{"name":"square","count":1,"percent":33.33}],
 "formats":[{"name":"PNG","count":2,"percent":66.67,"bytes":8000000000,"bytes_percent":88.89},{"name":"Other","count":1,"percent":33.33,"bytes":1000000000,"bytes_percent":11.11}],
 "other_formats":{"ICO":1},
 "failures":[{"path":"C:/pics/bad.jpg","status":"corrupted","error":"not a valid image"}],"failures_truncated":false}})JSON";

const char *kEmpty = R"JSON({"source":"files","selected":0,"analytics":{"denominator":0,
 "collection":{"total":0,"analyzed":0,"failed":0,"failed_by_status":{},"duplicates_ignored":0,"total_bytes":0,"size_known":0,"size_unknown":0,"failed_bytes":0,
  "average_bytes":null,"largest":null,"smallest":null},
 "resolution":{"total_pixels":0,"total_megapixels":0.0,"average_width":null,"average_height":null,"average_megapixels":null,"highest":null,"lowest":null,"most_common":null},
 "categories":[{"name":"HD","count":0,"percent":null}],"orientation":[],"formats":[],"other_formats":{},"failures":[],"failures_truncated":false}})JSON";

QJsonObject parse(const char *json) { return QJsonDocument::fromJson(json).object(); }
}  // namespace

class TstAnalytics : public QObject
{
    Q_OBJECT
private slots:
    void parsesEngineValuesExactly()
    {
        const AnalyticsData d = AnalyticsModel::fromJson(parse(kResult).value("analytics").toObject());
        QVERIFY(d.valid);
        QCOMPARE(d.total, 5);
        QCOMPARE(d.failed, 2);
        QCOMPARE(d.totalBytes, qint64(9000000000));  // beyond 32 bit
        QCOMPARE(d.largest.fileSize, qint64(6000000000));
        QCOMPARE(d.highest.width, 3840);
        QCOMPARE(d.commonTied, qint64(3));
        QCOMPARE(d.categories.size(), 4);
    }

    void nullIsNotAvailableNeverZero()
    {
        const AnalyticsData d = AnalyticsModel::fromJson(parse(kEmpty).value("analytics").toObject());
        QVERIFY(d.valid);
        QVERIFY(d.averageWidth < 0 && d.averageBytes < 0);
        QVERIFY(!d.highest.valid);
        QCOMPARE(AnalyticsModel::bytesText(d.averageBytes), AnalyticsModel::notAvailable());
        QCOMPARE(AnalyticsModel::percentText(d.categories[0].percent), QStringLiteral("—"));
        QVERIFY(!AnalyticsModel::fromJson(QJsonObject()).valid);
    }

    void formatting()
    {
        QCOMPARE(AnalyticsModel::bytesText(1536), QStringLiteral("1.50 KB"));
        QCOMPARE(AnalyticsModel::bytesText(0), QStringLiteral("0 B"));
        QCOMPARE(AnalyticsModel::percentText(33.333), QStringLiteral("33.3%"));
        QCOMPARE(AnalyticsModel::megapixelsText(2.0736), QStringLiteral("2.07 MP"));
        QCOMPARE(AnalyticsModel::sizeText(1920, 1080), QStringLiteral("1,920 × 1,080"));
        QCOMPARE(AnalyticsModel::statusText("invalid_metadata"), QStringLiteral("Invalid dimensions"));
    }

    void chartMath()
    {
        QCOMPARE(ShareChart::fraction(-1), 0.0);
        QCOMPARE(ShareChart::fraction(50), 0.5);
        QCOMPARE(ShareChart::fraction(250), 1.0);
        QCOMPARE(PixelSpan::position(1, 1, 100), 0.0);
        QCOMPARE(PixelSpan::position(100, 1, 100), 1.0);
        QVERIFY(qAbs(PixelSpan::position(10, 1, 100) - 0.5) < 1e-9);
        QCOMPARE(PixelSpan::position(5, 5, 5), 0.5);
    }

    void panelEmptyStateThenData()
    {
        AnalyticsPanel panel;
        panel.resize(900, 700);
        panel.show();
        QVERIFY(!panel.hasData());
        QVERIFY(!panel.findChild<MetricCard *>("tile.total")->isVisible());  // content hidden: empty state shows
        panel.showAnalytics(parse(kResult));
        QVERIFY(panel.hasData());
        auto text = [&](const char *k) { return panel.findChild<MetricCard *>(k)->valueText(); };
        QCOMPARE(text("tile.total"), QStringLiteral("5"));
        QCOMPARE(text("tile.analyzed"), QStringLiteral("3"));
        QCOMPARE(text("tile.failed"), QStringLiteral("2"));
        QCOMPARE(text("tile.storage"), QStringLiteral("8.38 GB"));
        QCOMPARE(text("tile.avgwidth"), QStringLiteral("1980.0 px"));
        QCOMPARE(text("tile.totalmp"), QStringLiteral("8.31 MP"));
        QCOMPARE(text("tile.avgmp"), QStringLiteral("2.77 MP"));
        QVERIFY(text("tile.common").startsWith(QStringLiteral("3,840 × 2,160")));
        QVERIFY(panel.sourceText().contains(QStringLiteral("subfolders included")));
        const auto charts = panel.findChildren<ShareChart *>();
        QCOMPARE(charts.size(), 3);
        QVERIFY(charts[0]->rowText(1).contains(QStringLiteral("Full HD: 1 images, 33.3%")));
        // failures are visible, not hidden
        QVERIFY(panel.findChildren<QLabel *>().size() > 0);
        bool sawBanner = false;
        for (QLabel *l : panel.findChildren<QLabel *>())
            if (l->text().contains(QStringLiteral("2 of 5 images could not be analyzed")))
                sawBanner = true;
        QVERIFY(sawBanner);
    }

    void panelEmptyCollectionShowsNotAvailable()
    {
        AnalyticsPanel panel;
        panel.showAnalytics(parse(kEmpty));
        QCOMPARE(panel.findChild<MetricCard *>("tile.avgsize")->valueText(), AnalyticsModel::notAvailable());
        QCOMPARE(panel.findChild<MetricCard *>("tile.avgmp")->valueText(), AnalyticsModel::notAvailable());
    }

    void navigationAndRefreshSignals()
    {
        AnalyticsPanel panel;
        panel.show();
        panel.showAnalytics(parse(kResult));
        QSignalSpy props(&panel, &AnalyticsPanel::propertiesRequested);
        QSignalSpy refresh(&panel, &AnalyticsPanel::refreshRequested);
        panel.findChild<QWidget *>(QStringLiteral("Highest resolution"))->findChildren<QPushButton *>().first()->click();
        QCOMPARE(props.size(), 1);
        QCOMPARE(props.first().first().toString(), QStringLiteral("C:/pics/big.png"));
        panel.setRefreshEnabled(true);
        for (QPushButton *b : panel.findChildren<QPushButton *>())
            if (b->text() == QStringLiteral("Refresh Analytics"))
                b->click();
        QCOMPARE(refresh.size(), 1);
        panel.setStale(true);
        QVERIFY(panel.isStale());
        panel.showAnalytics(parse(kResult));
        QVERIFY(!panel.isStale());
    }

    void requestUsesAnalyticsCommandAndTxtFlag()
    {
        ScanRequest r;
        r.folder = "C:/x";
        r.command = "analytics";
        QCOMPARE(r.commandArguments().first(), QStringLiteral("analytics"));
        ScanRequest s;
        s.folder = "C:/x";
        s.txtPath = "C:/r.txt";
        QVERIFY(!s.commandArguments().contains("--txt-analytics"));
        s.txtAnalytics = true;
        QVERIFY(s.commandArguments().contains("--txt-analytics"));
        s.txtPath.clear();
        QVERIFY(!s.commandArguments().contains("--txt-analytics"));
    }

    void railHasAnalyticsAvailableFromStart()
    {
        PhaseRail rail;
        QVERIFY(rail.isPhaseAvailable(PhaseRail::Analytics));
        QVERIFY(!rail.isPhaseAvailable(PhaseRail::Readout));
    }
};

QTEST_MAIN(TstAnalytics)
#include "tst_analytics.moc"
