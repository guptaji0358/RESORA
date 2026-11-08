// Developer tool (not a ctest): renders RESORA's windows and dialogs with sample data to PNG files on
// the real Windows platform plugin, so layout can be inspected visually.
// Usage: grab_screens <outdir> [analytics.json] [scale-note-only]
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>

#include "backend/duplicatesmodel.h"
#include "backend/propertiesmodel.h"
#include "locations/locationstore.h"
#include "mainwindow.h"
#include "ui/theme.h"
#include "widgets/aboutdialog.h"
#include "widgets/analyticspanel.h"
#include <QJsonArray>
#include <QLayout>

#include "backend/comparison.h"
#include "widgets/collapsiblesection.h"
#include "widgets/comparisontable.h"
#include "widgets/comparisonworkspace.h"
#include "widgets/insightsview.h"
#include "widgets/duplicatefinder.h"
#include "widgets/importdialog.h"
#include "widgets/progresspanel.h"
#include "widgets/propertiesdialog.h"
#include "widgets/resultspanel.h"
#include "widgets/settingsdialog.h"
#include "widgets/sourcepicker.h"

namespace {
QString gOut;

void pump(int ms)
{
    QTimer t;
    t.setSingleShot(true);
    t.start(ms);
    while (t.isActive())
        QApplication::processEvents(QEventLoop::AllEvents, 20);
}

void shot(QWidget *w, const QString &name)
{
    pump(50);
    w->grab().toImage().save(gOut + "/" + name + ".png");
}

const char *kDup = R"JSON({"stats":{"total_files":10,"empty_files":0,"exact_groups":2,"exact_files":5,"exact_extra_files":3,
 "exact_bytes":5000,"exact_savings_bytes":3000,"similar_groups":1,"similar_files":2,"similar_bytes":900,"skipped":1,
 "cache_hits":0,"flat_images":0},"settings":{"threshold":90,"similar":true},
 "exact_groups":[{"id":"e1","size":1000,"count":3,"total_bytes":3000,"savings_bytes":2000,"files":[
  {"path":"C:/Photos/Holiday/IMG_20240812_093412.png","name":"IMG_20240812_093412.png","folder":"C:/Photos/Holiday","format":"PNG","width":4032,"height":3024,"file_size":1000},
  {"path":"C:/Photos/Backup/IMG_20240812_093412 (1).png","name":"IMG_20240812_093412 (1).png","folder":"C:/Photos/Backup","format":"PNG","width":4032,"height":3024,"file_size":1000},
  {"path":"C:/b/3.png","name":"3.png","folder":"C:/b","format":"PNG","width":4032,"height":3024,"file_size":1000}]},
  {"id":"e2","size":1000,"count":2,"total_bytes":2000,"savings_bytes":1000,"files":[
  {"path":"C:/a/1.png","name":"1.png","folder":"C:/a","format":"PNG","width":10,"height":10,"file_size":1000},
  {"path":"C:/a/2.png","name":"2.png","folder":"C:/a","format":"PNG","width":10,"height":10,"file_size":1000}]}],
 "similar_groups":[{"id":"s1","count":2,"category":"Very similar","lowest_similarity":95.3,"total_bytes":900,"files":[
  {"path":"C:/a/x.png","name":"x.png","format":"PNG","width":20,"height":20,"file_size":500,"similarity":100.0,"category":"Nearly identical","anchor":true,"identical_copies":0},
  {"path":"C:/a/y.jpg","name":"y.jpg","format":"JPEG","width":10,"height":10,"file_size":400,"similarity":95.3,"category":"Very similar","anchor":false,"identical_copies":0}]}],
 "skipped":[{"path":"C:/a/bad.png","status":"corrupted","error":"x"}],"skipped_truncated":false})JSON";

const char *kProps = R"({"path":"C:/Photos/Holiday/IMG_20240812_093412.jpg","name":"IMG_20240812_093412.jpg","status":"ok","error":null,"file_size":4812345,
 "format":"JPEG","warnings":["Embedded thumbnail ignored"],"layout":"landscape","aspect_ratio":1.778,"aspect_label":"16:9","megapixels":12.19,"resolution":"UHD 4K",
 "color":{"mode":"RGB","mode_name":"RGB","channels":3,"bits_per_channel":8,"has_alpha":false,"icc_profile":"sRGB IEC61966-2.1"},
 "dpi":null,"exif":{"available":true,"fields":[],"gps_present":false,"orientation":1},"frames":null,
 "dimensions":{"stored_width":4032,"stored_height":3024,"width":4032,"height":3024,"orientation_tag":1,"orientation_label":"Normal","exif_rotated":false}})";
QJsonObject sampleProps(const QString &name, int w, int h, qint64 bytes, const QString &label, bool exif, bool icc)
{
    QJsonObject o = QJsonDocument::fromJson(kProps).object();
    o["name"] = name;
    o["path"] = "C:/Photos/" + name;
    o["file_size"] = bytes < 0 ? QJsonValue() : QJsonValue(bytes);
    o["aspect_ratio"] = h > 0 ? double(w) / h : 0.0;
    o["aspect_label"] = label;
    o["megapixels"] = double(w) * h / 1e6;
    QJsonObject d = o["dimensions"].toObject();
    d["width"] = w;
    d["height"] = h;
    o["dimensions"] = d;
    QJsonObject c = o["color"].toObject();
    c["icc_profile"] = icc ? QJsonValue("sRGB IEC61966-2.1") : QJsonValue();
    o["color"] = c;
    o["exif"] = QJsonObject{{"available", exif}, {"fields", QJsonArray()}, {"gps_present", false}, {"orientation", QJsonValue()}};
    return o;
}

// Image Comparison Insights in several situations and widths (whole content, not the viewport).
void grabInsights()
{
    struct Scenario { QString name; QJsonObject a, b; };
    const QList<Scenario> scenarios = {
        {"different", sampleProps("IMG_0412.jpg", 4032, 3024, 4812345, "4:3", true, true), sampleProps("scan_final.png", 1920, 1080, 1250000, "16:9", false, false)},
        {"identical", sampleProps("a.png", 1920, 1080, 500000, "16:9", true, true), sampleProps("b.png", 1920, 1080, 500000, "16:9", true, true)},
        {"sameaspect", sampleProps("photo_big.jpg", 3840, 2160, 3500000, "16:9", true, false), sampleProps("photo_small.jpg", 960, 540, 90000, "16:9", false, false)},
        {"invalid", sampleProps("zero.png", 0, 0, 0, "", false, false), sampleProps("nosize.png", 800, 600, -1, "4:3", false, false)},
        {"extreme", sampleProps("huge.tif", 12000, 9000, 90000000, "4:3", true, true), sampleProps("icon.png", 32, 24, 1500, "4:3", false, false)},
    };
    for (const Scenario &s : scenarios)
        for (int width : {900, 520, 340}) {
            ComparisonTable t;
            t.setComparison(ComparisonModel::compare(s.a, s.b));
            t.insights()->explanations()->setExpanded(width == 900);
            t.resize(width, 400);
            pump(100);
            t.resize(width, qMax(400, t.layout()->hasHeightForWidth() ? t.layout()->heightForWidth(width) : t.sizeHint().height()));
            pump(100);
            t.grab().save(gOut + QString("/insights_%1_%2.png").arg(s.name).arg(width));
        }
}
}  // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Theme::applyPalette(app);
    gOut = argc > 1 ? argv[1] : ".";
    QDir().mkpath(gOut);
    QTemporaryDir tmp;
    const QStringList sizes = {"1180x800", "960x640"};

    if (argc > 2 && QString(argv[2]) == "insights") {  // quick path: only the Insights renders
        grabInsights();
        return 0;
    }
    if (argc > 2 && QString(argv[2]) == "dup") {  // quick path: Duplicates results states (one group / many / empty)
        const QJsonObject full = QJsonDocument::fromJson(kDup).object();
        QJsonObject one = full, none = full;
        one["exact_groups"] = QJsonArray{full["exact_groups"].toArray().at(1)};
        one["similar_groups"] = QJsonArray();
        none["exact_groups"] = QJsonArray();
        none["similar_groups"] = QJsonArray();
        const QList<QPair<QString, QJsonObject>> cases = {{"one", one}, {"many", full}, {"empty", none}};
        for (const auto &c : cases) {
            MainWindow w(nullptr, tmp.path() + "/locs.json");
            w.resize(1180, 800);
            w.show();
            w.showTool(Tool::Duplicates);
            w.duplicates()->showResult(DuplicatesModel::fromJson(c.second));
            pump(400);
            shot(&w, "dup_" + c.first);
            if (c.first == "many") {
                w.duplicates()->showKind(false);
                pump(300);
                shot(&w, "dup_many_similar");
            }
        }
        return 0;
    }
    QStringList imgs;
    for (int i = 0; i < 2; ++i) {
        QImage im(i ? 1280 : 1920, i ? 720 : 1080, QImage::Format_RGB32);
        QPainter p(&im);
        p.fillRect(im.rect(), i ? QColor(40, 90, 140) : QColor(140, 60, 40));
        p.setPen(Qt::white);
        p.setFont(QFont("Segoe UI", 60));
        p.drawText(im.rect(), Qt::AlignCenter, QString("Sample %1").arg(i));
        p.end();
        const QString f = tmp.path() + QString("/sample_%1.png").arg(i);
        im.save(f);
        imgs << f;
    }
    const QString locFile = tmp.path() + "/locs.json";
    ImportSelection sel;
    sel.kind = ImportSelection::Folder;
    sel.folder = "C:/Users/lenovo/Pictures/Holiday 2024 — Italy";

    QJsonObject analytics;
    if (argc > 2) {
        QFile f(argv[2]);
        if (f.open(QIODevice::ReadOnly))
            analytics = QJsonDocument::fromJson(f.readAll()).object();
    }

    for (const QString &sz : sizes) {
        const QStringList wh = sz.split('x');
        MainWindow w(nullptr, locFile);
        w.resize(wh[0].toInt(), wh[1].toInt());
        w.show();
        pump(300);
        QStackedWidget *stack = nullptr;  // the Analyzer's phase stack (page 3 = hosted Properties)
        for (auto *st : w.findChildren<QStackedWidget *>())
            if (st->count() == 4 && st->widget(3) == w.properties())
                stack = st;
        shot(&w, "main_empty_" + sz);
        w.sourcePicker()->setSelection(sel);
        pump(100);
        shot(&w, "main_source_" + sz);

        stack->setCurrentIndex(1);
        auto *pp = w.findChild<ProgressPanel *>();
        pp->setStarting();
        pp->setProgress(412, 1200, "C:/Users/lenovo/Pictures/Holiday 2024 — Italy/Rome/IMG_0412.jpg");
        pump(200);
        shot(&w, "main_scan_" + sz);
        pp->setFailed("Backend failed to start", "Traceback line 1\nline 2");
        pump(100);
        shot(&w, "main_scanfail_" + sz);

        stack->setCurrentIndex(2);
        QJsonObject summary{{"total", 1200}, {"analyzed", 1190}, {"failed", 10},
                            {"by_resolution", QJsonObject{{"Full HD", 800}, {"HD", 100}, {"UHD 4K", 200}, {"SD", 40}, {"Custom (below HD)", 50}}}};
        w.findChild<ResultsPanel *>()->showResult(QJsonObject{{"summary", summary}, {"skipped_non_images", 2}}, true, true);
        pump(200);
        shot(&w, "main_results_" + sz);

        w.showTool(Tool::Analytics);
        w.analyticsPanel()->clear();
        pump(100);
        shot(&w, "main_analytics_empty_" + sz);
        if (!analytics.isEmpty())
            w.analyticsPanel()->showAnalytics(analytics);
        pump(300);
        shot(&w, "main_analytics_" + sz);
        {  // whole panel, not only the visible viewport (both views)
            auto *ap = w.analyticsPanel();
            for (int v = 0; v < 2; ++v) {
                ap->setViewIndex(v);
                pump(200);
                ap->grab().save(gOut + QString("/panel_analytics_v%1_%2.png").arg(v).arg(sz));
            }
            ap->setViewIndex(0);
        }

        w.showTool(Tool::Duplicates);  // hosted page, as the user sees it
        pump(200);
        shot(&w, "main_dup_empty_" + sz);
        w.duplicates()->showResult(DuplicatesModel::fromJson(QJsonDocument::fromJson(kDup).object()));
        pump(300);
        shot(&w, "main_dup_results_" + sz);

        w.comparison()->setImage(ComparisonWorkspace::Left, imgs[0]);
        w.comparison()->setImage(ComparisonWorkspace::Right, imgs[1]);
        w.showTool(Tool::Compare);
        pump(1500);
        shot(&w, "main_compare_" + sz);
        w.comparison()->clearImage(ComparisonWorkspace::Left);
        w.comparison()->clearImage(ComparisonWorkspace::Right);
        pump(200);
        shot(&w, "main_compare_empty_" + sz);
    }

    MainWindow host(nullptr, locFile);
    host.resize(1180, 800);
    host.show();
    pump(200);
    LocationStore store(tmp.path() + "/store.json");
    store.add("Holiday photos 2024", "C:/Users/lenovo/Pictures/Holiday 2024 — Italy/Rome/Colosseum/Day 2");
    store.add("Work", tmp.path());
    store.add("Missing drive", "Z:/Gone/Away");
    {
        SettingsDialog d(&store, &host);
        d.show();
        pump(300);
        shot(&d, "dlg_settings");
        LocationStore empty(tmp.path() + "/none.json");
        SettingsDialog e(&empty, &host);
        e.show();
        pump(300);
        shot(&e, "dlg_settings_empty");
    }
    {
        AboutDialog d(&host);
        d.show();
        shot(&d, "dlg_about");
    }
    {
        ImportDialog d(tmp.path(), &host, &store);
        d.show();
        shot(&d, "dlg_import");
    }
    {
        PropertiesDialog d(QStringList{}, &host);
        d.show();
        pump(200);
        shot(&d, "dlg_props_empty");
        const ImageProperties p = PropertiesModel::fromJson(QJsonDocument::fromJson(kProps).object());
        d.showImages({p, p});
        pump(300);
        shot(&d, "dlg_props");
    }
    {
        DuplicateFinder d(imgs, QString(), QString(), &host);
        d.show();
        pump(300);
        shot(&d, "dlg_dup_setup");
        d.showResult(DuplicatesModel::fromJson(QJsonDocument::fromJson(kDup).object()));
        pump(300);
        shot(&d, "dlg_dup_results");
        d.showKind(false);
        pump(200);
        shot(&d, "dlg_dup_similar");
    }
    return 0;
}
