#include <QCheckBox>
#include <QDir>
#include <QLabel>
#include <QTimer>
#include <QJsonObject>
#include <QProgressBar>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

#include "backend/backendlocator.h"
#include "backend/scanrequest.h"
#include "mainwindow.h"
#include "ui/brand.h"
#include "widgets/aboutdialog.h"
#include "widgets/appheader.h"
#include "widgets/distributionbar.h"
#include "backend/importselection.h"
#include "widgets/importdialog.h"
#include "widgets/sourcepicker.h"
#include "widgets/viewfinder.h"
#include "ui/uiformat.h"
#include "widgets/metriccard.h"
#include "widgets/chamferbutton.h"
#include "widgets/flowlayout.h"
#include "widgets/progresspanel.h"
#include "widgets/reportselector.h"
#include "widgets/resultspanel.h"

namespace {
void touch(const QString &path)
{
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
}
}  // namespace

class TstWidgets : public QObject
{
    Q_OBJECT
private slots:
    void importFolderSelection()
    {
        QTemporaryDir dir;
        const auto ok = ImportSelection::fromFolder(dir.path());
        QVERIFY(ok.isValid());
        QCOMPARE(ok.kind, ImportSelection::Folder);
        QVERIFY(!ImportSelection::fromFolder("").isValid());
        QVERIFY(!ImportSelection::fromFolder(dir.filePath("missing")).isValid());
        touch(dir.filePath("f.txt"));
        QVERIFY(!ImportSelection::fromFolder(dir.filePath("f.txt")).isValid());
        QCOMPARE(ImportSelection::fromPaths({dir.path()}).kind, ImportSelection::Folder);
    }

    void importFileSelectionClassifiesEverything()
    {
        QTemporaryDir dir;
        for (const char *n : {"a.png", "b.JPG", "c.txt"})
            touch(dir.filePath(n));
        const auto one = ImportSelection::fromFiles({dir.filePath("a.png")});
        QCOMPARE(one.kind, ImportSelection::SingleImage);
        QCOMPARE(one.title(), QString("a.png"));

        const auto mixed = ImportSelection::fromFiles({dir.filePath("a.png"), dir.filePath("A.PNG"), dir.filePath("b.JPG"),
                                                       dir.filePath("c.txt"), dir.filePath("gone.png"), "", dir.path()});
        QCOMPARE(mixed.kind, ImportSelection::MultipleImages);
        QCOMPARE(mixed.files.size(), 2);
        QCOMPARE(mixed.duplicates, 1);
        QCOMPARE(mixed.unsupported.size(), 1);
        QCOMPARE(mixed.missing.size(), 2);  // missing file + a directory (not a file)
        QCOMPARE(mixed.notes().size(), 3);  // nothing is dropped silently
        QVERIFY(mixed.isValid());

        const auto none = ImportSelection::fromFiles({dir.filePath("c.txt"), dir.filePath("gone.png")});
        QVERIFY(!none.isValid());
        QVERIFY(!none.problem.isEmpty());
        QVERIFY(!none.notes().isEmpty());
        QVERIFY(!ImportSelection::fromFiles({}).isValid());
    }

    void importHandlesLongPathsAndLargeSelections()
    {
        QTemporaryDir dir;
        QStringList many;
        for (int i = 0; i < 5000; ++i)
            many << dir.filePath(QString("img%1.png").arg(i));
        const auto big = ImportSelection::fromFiles(many);  // none exist: all reported, none dropped
        QCOMPARE(big.missing.size(), 5000);
        QVERIFY(big.notes().first().size() < 400);           // message stays short
        QString deep = dir.path();
        for (int i = 0; i < 4; ++i)
            deep += "/" + QString(60, QChar('d'));
        QVERIFY(QDir().mkpath(deep));
        touch(deep + "/x.png");
        QVERIFY(ImportSelection::fromFiles({deep + "/x.png"}).isValid());
    }

    void importDialogResults()
    {
        QTemporaryDir dir;
        touch(dir.filePath("a.png"));
        ImportDialog d(dir.path());
        d.applyFiles({dir.filePath("a.png")});
        QCOMPARE(d.result(), int(QDialog::Accepted));
        QCOMPARE(d.selection().kind, ImportSelection::SingleImage);
        ImportDialog f(dir.path());
        f.applyFolder(dir.path());
        QCOMPARE(f.selection().kind, ImportSelection::Folder);
        QVERIFY(d.findChild<QPushButton *>("importTile_folder"));
        QVERIFY(d.findChild<QPushButton *>("importTile_single"));
        QVERIFY(d.findChild<QPushButton *>("importTile_multiple"));
    }

    void ctrlOOpensImportDialog()
    {
        MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowActive(&w));
        bool opened = false;
        QTimer::singleShot(200, [&] {
            for (auto *top : QApplication::topLevelWidgets())
                if (auto *dlg = qobject_cast<ImportDialog *>(top)) {
                    opened = true;
                    dlg->reject();
                }
        });
        QTest::keyClick(&w, Qt::Key_O, Qt::ControlModifier);
        QVERIFY(opened);
    }

    void sourcePickerShowsSelection()
    {
        QTemporaryDir dir;
        touch(dir.filePath("a.png"));
        touch(dir.filePath("b.png"));
        SourcePicker p;
        QVERIFY(!p.validationError().isEmpty());
        p.setSelection(ImportSelection::fromFiles({dir.filePath("a.png"), dir.filePath("b.png"), dir.filePath("c.txt")}));
        QVERIFY(p.validationError().isEmpty());
        QStringList texts;
        for (auto *l : p.findChildren<QLabel *>())
            texts << l->text();
        const QString all = texts.join('|');
        QVERIFY(all.contains("MULTIPLE IMAGES"));
        QVERIFY(all.contains("2 images"));
        QVERIFY(all.contains("c.txt") || all.contains("left out"));
    }

    void reportSelectorExtensionAndValidation()
    {
        QTemporaryDir dir;
        ReportSelector csv("CSV", "csv", "CSV files");
        QVERIFY(!csv.isWanted());
        QVERIFY(csv.path().isEmpty());
        QVERIFY(csv.validationError().isEmpty());       // disabled = nothing to validate
        csv.setWanted(true);
        QVERIFY(!csv.validationError().isEmpty());      // enabled but empty
        csv.setPath(dir.filePath("out"));
        QVERIFY(csv.path().endsWith("out.csv"));        // extension appended
        csv.setPath(dir.filePath("OUT.CSV"));
        QVERIFY(csv.path().endsWith("OUT.CSV"));        // existing extension kept
        csv.setPath(dir.filePath("new/sub/r.csv"));
        QVERIFY(csv.validationError().isEmpty());       // missing parents are allowed
        csv.setPath(dir.path());
        QVERIFY(!csv.validationError().isEmpty());      // a directory is not a file
    }

    void scanRequestForSelectedFilesUsesParamsFile()
    {
        ScanRequest r;
        r.files = {"C:/a.png", "C:/b.png"};
        r.paramsFile = "C:/tmp/p.json";
        r.csvPath = "a.csv";
        QCOMPARE(r.commandArguments(), (QStringList{"--params-file", "C:/tmp/p.json", "scan", "--csv", "a.csv"}));
        const auto doc = QJsonDocument::fromJson(r.paramsJson());
        QCOMPARE(doc.object().value("files").toArray().size(), 2);
    }

    void scanRequestArguments()
    {
        ScanRequest r;
        r.folder = "C:/imgs";
        QCOMPARE(r.commandArguments(), (QStringList{"scan", "C:/imgs", "--recursive"}));
        r.recursive = false;
        r.csvPath = "a.csv";
        QCOMPARE(r.commandArguments(), (QStringList{"scan", "C:/imgs", "--csv", "a.csv"}));
        r.txtPath = "a.txt";
        QCOMPARE(r.commandArguments(),
                 (QStringList{"scan", "C:/imgs", "--csv", "a.csv", "--txt", "a.txt"}));
    }

    void locatorReportsBadEnvironment()
    {
        QProcessEnvironment env;
        env.insert("IMGAN_BACKEND_DIR", "C:/definitely/not/here");
        QVERIFY(!locateBackend(env, QCoreApplication::applicationDirPath()).ok());
    }

    void uiFormatHelpers()
    {
        QCOMPARE(UiFormat::count(1234567), QString("1,234,567"));
        QCOMPARE(UiFormat::percent(1, 4), 25);
        QCOMPARE(UiFormat::percent(9, 4), 100);   // clamped
        QCOMPARE(UiFormat::percent(1, 0), -1);    // unknown total: no invented percentage
        QCOMPARE(UiFormat::progressSummary(1, 0), QString());
        QCOMPARE(UiFormat::progressSummary(5, 1000), QString("5 of 1,000 files (0%)"));
    }

    void progressPanelStates()
    {
        ProgressPanel p;
        QCOMPARE(p.state(), ProgressPanel::State::Idle);
        QVERIFY(!p.isIndeterminate());
        p.setStarting();
        QVERIFY(p.isIndeterminate());
        p.setProgress(3, 0, "discovering");           // no total -> stays indeterminate
        QVERIFY(p.isIndeterminate());
        p.setProgress(3, 10, "a.png");
        QVERIFY(!p.isIndeterminate());
        QCOMPARE(p.findChild<QProgressBar *>()->value(), 3);
        p.setCancelling();
        p.setProgress(4, 10, "late");                 // ignored while cancelling
        QCOMPARE(p.findChild<QProgressBar *>()->value(), 3);
        p.setCancelled("x");
        QCOMPARE(p.state(), ProgressPanel::State::Cancelled);
        p.setStarting();
        p.setFailed("boom");                          // indeterminate bar is not left spinning
        QVERIFY(!p.isIndeterminate());
        QCOMPARE(p.state(), ProgressPanel::State::Failed);
    }

    void resultsPanelShowsMetrics()
    {
        ResultsPanel r;
        QJsonObject summary{{"total", 1200}, {"analyzed", 1190}, {"failed", 10},
                            {"by_resolution", QJsonObject{{"Full HD", 1000}}}};
        r.showResult(QJsonObject{{"summary", summary}, {"skipped_non_images", 2}}, true, false);
        QStringList values;
        for (auto *m : r.findChildren<MetricCard *>())
            values << m->valueText();
        QVERIFY(values.contains("1,200"));
        QVERIFY(values.contains("1,190"));
        QVERIFY(values.contains("10"));
        // CSV was requested but unconfirmed: its Open button must be disabled.
        for (auto *b : r.findChildren<QPushButton *>())
            if (b->text() == "Open Report" && b->isVisibleTo(&r)) QVERIFY(!b->isEnabled());
    }

    void distributionBarShares()
    {
        QCOMPARE(DistributionBar::share(1, 4), 0.25);
        QCOMPARE(DistributionBar::share(5, 0), 0.0);
        DistributionBar d;
        d.setEntries({{"Full HD", 3}, {"4K", 1}, {"Zero", 0}});
        QCOMPARE(d.rowCount(), 2);                    // zero counts are not drawn
        d.setEntries({});
        QCOMPARE(d.rowCount(), 0);
    }

    void ringGaugeAndHeaderStates()
    {
        Viewfinder g;
        g.setFraction(1.7);
        QCOMPARE(g.fraction(), 1.0);                  // clamped
        g.setFraction(-5);
        QCOMPARE(g.fraction(), -1.0);                 // indeterminate
        ProgressPanel p;
        AppHeader h;
        connect(&p, &ProgressPanel::stateChanged, &h, &AppHeader::setState);
        p.setStarting();
        QCOMPARE(h.stateText(), QString("Starting"));
        p.setFailed("x");
        QCOMPARE(h.stateText(), QString("Failed"));
    }

    void brandIdentity()
    {
        QCOMPARE(Brand::name(), QString("RESORA"));
        QCOMPARE(Brand::description(), QString("Image Resolution Analyzer"));
        QCOMPARE(Brand::tagline(), QString("See every pixel clearly."));
        QVERIFY(!Brand::icon().isNull());
        QVERIFY(Brand::icon().availableSizes().contains(QSize(32, 32)));
        MainWindow w;
        QVERIFY(w.windowTitle().startsWith("RESORA"));
        AboutDialog d;
        QVERIFY(d.windowTitle().contains("RESORA"));
    }

    void toolsAreIndependentAndRouted()
    {
        MainWindow w;
        QCOMPARE(w.currentTool(), Tool::Analyzer);
        for (Tool t : {Tool::Analytics, Tool::Compare, Tool::Duplicates, Tool::Analyzer}) {  // each opens first, no scan
            w.showTool(t);
            QCOMPARE(w.currentTool(), t);
        }
        QVERIFY(w.duplicates() && w.analyticsSource() && w.properties());
        w.route({Tool::Compare, {QStringLiteral("nope-a.png"), QStringLiteral("nope-b.png")}});
        QCOMPARE(w.currentTool(), Tool::Compare);
        QVERIFY(w.isComparing());
        w.route({Tool::Duplicates, {}});
        QCOMPARE(w.currentTool(), Tool::Duplicates);
    }

    void mainWindowInitialState()
    {
        MainWindow w;
        auto *start = w.findChild<QPushButton *>();
        const auto buttons = w.findChildren<QPushButton *>();
        QPushButton *startBtn = nullptr, *cancelBtn = nullptr;
        for (auto *b : buttons) {
            if (b->text() == "Start Scan") startBtn = b;
            if (b->text() == "Cancel Scan") cancelBtn = b;
        }
        Q_UNUSED(start)
        QVERIFY(startBtn && cancelBtn);
        QVERIFY(startBtn->isEnabled());
        QVERIFY(!cancelBtn->isEnabled());
        QVERIFY(!w.isScanning());
        // Include subfolders defaults on.
        bool found = false;
        for (auto *c : w.findChildren<QCheckBox *>())
            if (c->text() == "Include subfolders") { found = true; QVERIFY(c->isChecked()); }
        QVERIFY(found);
        // Starting with nothing imported must not start a scan.
        startBtn->click();
        QVERIFY(!w.isScanning());
        QVERIFY(startBtn->isEnabled());
    }

    void flowLayoutWrapsInsteadOfSqueezing()
    {
        QWidget host;
        auto *flow = new FlowLayout(&host);
        QList<ChamferButton *> buttons;
        for (const char *t : {"Image Properties", "Analytics", "Compare Images", "New Scan"}) {
            buttons << new ChamferButton(QString::fromLatin1(t), &host);
            flow->addWidget(buttons.last());
        }
        host.resize(420, flow->heightForWidth(420));  // narrower than the four buttons side by side
        flow->activate();
        for (ChamferButton *b : buttons) {
            QCOMPARE(b->width(), b->sizeHint().width());      // never squeezed below the label
            QVERIFY(b->geometry().right() <= host.width());   // never past the edge
        }
        QVERIFY(buttons.last()->y() > buttons.first()->y());  // wrapped onto a new row
        QVERIFY(flow->heightForWidth(2000) < flow->heightForWidth(420));
    }
};

QTEST_MAIN(TstWidgets)
#include "tst_widgets.moc"
