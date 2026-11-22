#include <QJsonDocument>
#include <QLabel>
#include <QSignalSpy>
#include <QtTest>

#include "backend/dashboardmodel.h"
#include "widgets/analyticsdashboard.h"
#include "widgets/analyticspanel.h"
#include "widgets/barchart.h"
#include "widgets/donutchart.h"
#include "widgets/hbarchart.h"
#include "widgets/metriccard.h"
#include "widgets/segmentedswitch.h"

namespace {
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
AnalyticsData data(const char *json) { return AnalyticsModel::fromJson(parse(json).value("analytics").toObject()); }
}  // namespace

class TstDashboard : public QObject
{
    Q_OBJECT
private slots:
    void chartItemsUseEngineValues()
    {
        const AnalyticsData d = data(kResult);
        const auto cats = DashboardModel::categoryItems(d);
        QCOMPARE(cats.size(), 4);
        QCOMPARE(cats[1].label, QStringLiteral("Full HD"));
        QCOMPARE(cats[1].count, qint64(1));
        QCOMPARE(cats[1].percent, 33.33);  // engine percent, denominator = analyzed (3), not total (5)
        QCOMPARE(cats[0].count, qint64(0));  // zero classes are kept
        QCOMPARE(DashboardModel::orientationItems(d)[0].label, QStringLiteral("Landscape"));
        const auto st = DashboardModel::storageItems(d);
        QCOMPARE(st.size(), 2);
        QCOMPARE(st[0].value, 8000000000.0);
        QCOMPARE(st[0].percent, 88.89);
        QCOMPARE(st[0].secondary, 66.67);
        QVERIFY(st[0].tooltip.contains("88.9%"));
    }

    void emptyAndMissingDataAreNotZeroCharts()
    {
        const AnalyticsData d = data(kEmpty);
        QVERIFY(DashboardModel::categoryItems(d).isEmpty());
        QVERIFY(DashboardModel::storageItems(d).isEmpty());
        AnalyticsData noSize = data(kResult);
        noSize.sizeKnown = 0;  // sizes unknown must not draw a storage chart of zeros
        QVERIFY(DashboardModel::storageItems(noSize).isEmpty());
        QVERIFY(!DashboardModel::insights(d).isEmpty());
    }

    void chartMath()
    {
        QCOMPARE(DashboardModel::niceAxisMax(0), qint64(1));
        QCOMPARE(DashboardModel::niceAxisMax(7), qint64(10));
        QCOMPARE(DashboardModel::niceAxisMax(12), qint64(20));
        QCOMPARE(DashboardModel::niceAxisMax(51), qint64(100));
        QCOMPARE(DashboardModel::niceAxisMax(1000000), qint64(1000000));
        QCOMPARE(BarChart::barHeight(0, 10, 100), 0);
        QCOMPARE(BarChart::barHeight(5, 10, 100), 50);
        QCOMPARE(BarChart::barHeight(1, 1000000, 100), 2);  // a non-zero count stays visible
        QCOMPARE(DonutChart::sweep(1, 4), 90.0);
        QCOMPARE(DonutChart::sweep(5, 0), 0.0);
    }

    void dashboardIsDefaultAndSwitchingKeepsData()
    {
        AnalyticsPanel panel;
        panel.resize(900, 900);
        panel.show();
        panel.showAnalytics(parse(kResult));
        QCOMPARE(panel.viewIndex(), 0);
        QVERIFY(panel.findChild<MetricCard *>("dash.total")->isVisible());
        QVERIFY(!panel.findChild<MetricCard *>("tile.total")->isVisible());
        QCOMPARE(panel.findChild<MetricCard *>("dash.total")->valueText(), QStringLiteral("5"));
        QCOMPARE(panel.findChild<MetricCard *>("dash.storage")->valueText(), QStringLiteral("8.38 GB"));
        QCOMPARE(panel.findChild<MetricCard *>("dash.failed")->valueText(), QStringLiteral("2"));
        panel.setViewIndex(1);
        QVERIFY(panel.findChild<MetricCard *>("tile.total")->isVisible());
        QCOMPARE(panel.findChild<MetricCard *>("tile.total")->valueText(), QStringLiteral("5"));
        panel.setViewIndex(0);
        QVERIFY(panel.hasData());
        QCOMPARE(panel.findChild<MetricCard *>("dash.total")->valueText(), QStringLiteral("5"));
        // Same underlying numbers in both views.
        QCOMPARE(panel.findChild<MetricCard *>("dash.avgmp")->valueText(), panel.findChild<MetricCard *>("tile.avgmp")->valueText());
    }

    void emptyCollectionShowsEmptyStates()
    {
        AnalyticsPanel panel;
        panel.resize(900, 900);
        panel.show();
        panel.showAnalytics(parse(kEmpty));
        for (ChartBase *c : panel.findChildren<ChartBase *>())
            QVERIFY(c->isEmptyState());
        QCOMPARE(panel.findChild<MetricCard *>("dash.avgmp")->valueText(), AnalyticsModel::notAvailable());
        QCOMPARE(panel.findChild<MetricCard *>("dash.storage")->valueText(), AnalyticsModel::notAvailable());
        QVERIFY(!panel.grab().isNull());
    }

    void selectionRevealsDetailAndClearsOnNewData()
    {
        AnalyticsDashboard dash;
        dash.resize(900, 1400);
        dash.show();
        dash.setData(data(kResult));
        auto *bars = dash.findChild<BarChart *>();
        QSignalSpy spy(bars, &ChartBase::selectionChanged);
        bars->setSelected(1);
        QCOMPARE(bars->selected(), 1);
        QCOMPARE(spy.size(), 1);
        bool shown = false;
        for (QLabel *l : dash.findChildren<QLabel *>())
            shown |= l->text().startsWith(QStringLiteral("Full HD — 1 image"));
        QVERIFY(shown);
        dash.setData(data(kResult));
        QCOMPARE(bars->selected(), -1);
    }

    void keyboardAndSwitch()
    {
        SegmentedSwitch sw({"A", "B"});
        sw.show();
        QSignalSpy spy(&sw, &SegmentedSwitch::currentChanged);
        QTest::keyClick(&sw, Qt::Key_Right);
        QCOMPARE(sw.current(), 1);
        QTest::keyClick(&sw, Qt::Key_Right);  // clamps
        QCOMPARE(sw.current(), 1);
        QCOMPARE(spy.size(), 1);
    }

    void largeCollectionAndResizing()
    {
        AnalyticsData d = data(kResult);
        d.total = d.analyzed = 12000000;
        d.categories[1].count = 9999999;
        AnalyticsDashboard dash;
        dash.setData(d);
        for (const QSize &s : {QSize(1100, 1500), QSize(520, 1900), QSize(360, 2200)}) {
            dash.resize(s);
            dash.show();
            QVERIFY(!dash.grab().isNull());
            QVERIFY(dash.findChild<DonutChart *>()->height() >= dash.findChild<DonutChart *>()->heightForWidth(dash.findChild<DonutChart *>()->width()));
        }
        QCOMPARE(dash.findChild<MetricCard *>("dash.total")->valueText(), QStringLiteral("12,000,000"));
    }
};

QTEST_MAIN(TstDashboard)
#include "tst_dashboard.moc"
