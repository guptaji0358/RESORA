// MainWindow integration + session-replacement lifecycle tests (Phase 6). Offscreen; drives the modal
// dialogs from a zero-delay timer inside exec(); no coordinates, no animation timing.
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QShortcut>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <functional>

#include "backend/dashboardstate.h"
#include "mainwindow.h"
#include "widgets/analyticspanel.h"
#include "widgets/exportdialog.h"
#include "widgets/openresoradialog.h"
#include "widgets/relinkpanel.h"

using namespace dashboard;

namespace {
const char *kTemplate = R"JSON({"source":"folder","folder":"C:/pics","recursive":true,"duplicates_ignored":0,"analytics":{
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

bool put(const QString &p, const QByteArray &d)
{
    QDir().mkpath(QFileInfo(p).absolutePath());
    QFile f(p);
    return f.open(QIODevice::WriteOnly) && f.write(d) == d.size();
}

template <class T> QList<T *> dialogs()
{
    QList<T *> out;
    for (QWidget *w : QApplication::topLevelWidgets())
        if (auto *d = qobject_cast<T *>(w))
            out << d;
    return out;
}

const QKeySequence kExport(Qt::CTRL | Qt::SHIFT | Qt::Key_E);
const QKeySequence kOpen(Qt::CTRL | Qt::SHIFT | Qt::Key_O);
}  // namespace

class TstMainWindow : public QObject
{
    Q_OBJECT
    QTemporaryDir dir_;
    QString p(const QString &n) const { return dir_.filePath(n); }
    QJsonObject data() const
    {
        QByteArray j(kTemplate);
        j.replace("C:/pics", dir_.path().toUtf8() + "/pics");
        return QJsonDocument::fromJson(j).object();
    }
    void fire(MainWindow &w, const QKeySequence &k)
    {
        for (QShortcut *s : w.findChildren<QShortcut *>())
            if (s->key() == k) {
                emit s->activated();
                return;
            }
        QFAIL("shortcut not registered");
    }
    // Runs `fn` on the single open dialog of type T once exec() is spinning.
    template <class T> void drive(std::function<void(T *)> fn)
    {
        QTimer::singleShot(0, [fn] {
            auto l = dialogs<T>();
            QCOMPARE(l.size(), 1);
            fn(l.first());
        });
    }
    void exportFile(const QString &file, resoraui::Mode m)
    {
        AnalyticsPanel panel;
        panel.showAnalytics(data());
        ExportDialog dlg(State{panel.result(), panel.viewIndex()}, "t");
        dlg.setMode(m);
        dlg.setDestination(file);
        QVERIFY(dlg.startExport());
        QTRY_VERIFY(!dlg.isRunning());
        QVERIFY2(dlg.outcome().ok, qPrintable(dlg.outcome().message));
    }
    void openVia(MainWindow &w, const QString &file)
    {
        drive<OpenResoraDialog>([file](OpenResoraDialog *d) {
            d->setFile(file);
            QVERIFY(d->startOpen());
        });
        fire(w, kOpen);
    }
    static QString pathOf(const std::shared_ptr<Session> &s)
    {
        return runtimePaths(*s->current()).values().value(0);
    }

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init()
    {
        for (const QString &f : QDir(dir_.path()).entryList({"*.resora"}, QDir::Files))
            QFile::remove(p(f));  // exports must not hit the overwrite prompt
        put(p("pics/big.png"), QByteArray(5000, 'a'));
        put(p("pics/s.jpg"), QByteArray(900, 'b'));
    }

    void exportShortcutOpensOneDialog()
    {
        MainWindow w(nullptr, p("loc.json"));
        w.analyticsPanel()->showAnalytics(data());
        int seen = 0;
        drive<ExportDialog>([&](ExportDialog *d) {
            ++seen;
            fire(w, kExport);  // repeat while open: ignored
            QCOMPARE(dialogs<ExportDialog>().size(), 1);
            d->close();
        });
        fire(w, kExport);
        QCOMPARE(seen, 1);
        QVERIFY(dialogs<ExportDialog>().isEmpty());
    }

    void exportWithoutDataOpensNothing()
    {
        MainWindow w(nullptr, p("loc.json"));
        fire(w, kExport);  // would block in exec() if a dialog opened
        QVERIFY(dialogs<ExportDialog>().isEmpty());
    }

    void importShortcutOpensOneDialogAndRepeatIsGuarded()
    {
        MainWindow w(nullptr, p("loc.json"));
        int seen = 0;
        drive<OpenResoraDialog>([&](OpenResoraDialog *d) {
            ++seen;
            fire(w, kOpen);  // repeat while open: ignored
            QCOMPARE(dialogs<OpenResoraDialog>().size(), 1);
            d->close();
        });
        fire(w, kOpen);
        QCOMPARE(seen, 1);
        QVERIFY(dialogs<OpenResoraDialog>().isEmpty());
        for (int i = 0; i < 2; ++i) {  // sequential repeats: one fresh dialog each, none left over
            int n = 0;
            drive<OpenResoraDialog>([&](OpenResoraDialog *d) { ++n; d->close(); });
            fire(w, kOpen);
            QCOMPARE(n, 1);
            QVERIFY(dialogs<OpenResoraDialog>().isEmpty());
        }
        QVERIFY(!w.dashboardSession());
    }

    void successfulReplacementSwapsSessionAndReleasesOldCache()
    {
        exportFile(p("a.resora"), resoraui::Mode::Bundled);
        exportFile(p("b.resora"), resoraui::Mode::Bundled);
        MainWindow w(nullptr, p("loc.json"));
        openVia(w, p("a.resora"));
        auto a = w.dashboardSession();
        QVERIFY(a && a->current());
        const QString aFile = pathOf(a);
        QVERIFY(QFileInfo(aFile).isFile());
        std::weak_ptr<Session> weakA = a;
        a.reset();

        QString seenDuring;
        drive<OpenResoraDialog>([&](OpenResoraDialog *d) {
            seenDuring = QFileInfo(aFile).isFile() ? "alive" : "gone";  // old cache alive while B opens
            d->setFile(p("b.resora"));
            QVERIFY(d->startOpen());
        });
        fire(w, kOpen);
        QCOMPARE(seenDuring, QString("alive"));
        auto b = w.dashboardSession();
        QVERIFY(b && b->current());
        QVERIFY(weakA.expired());  // A released only after B was activated
        QVERIFY(!QFileInfo(aFile).exists());
        QVERIFY(QFileInfo(pathOf(b)).isFile());
        QVERIFY(w.analyticsPanel()->hasData());
    }

    void failedCancelledAndRelinkFailureKeepActiveDashboard()
    {
        exportFile(p("a.resora"), resoraui::Mode::Bundled);
        exportFile(p("l.resora"), resoraui::Mode::Linked);
        exportFile(p("c.resora"), resoraui::Mode::Bundled);
        put(p("bad.resora"), QByteArray(300, 'q'));
        MainWindow w(nullptr, p("loc.json"));
        openVia(w, p("a.resora"));
        auto a = w.dashboardSession();
        QVERIFY(a && a->current());
        const QString aFile = pathOf(a);
        const State before{w.analyticsPanel()->result(), w.analyticsPanel()->viewIndex()};
        auto unchanged = [&] {
            QCOMPARE(w.dashboardSession(), a);
            QVERIFY(QFileInfo(aFile).isFile());
            QVERIFY((State{w.analyticsPanel()->result(), w.analyticsPanel()->viewIndex()} == before));
            QVERIFY(dialogs<OpenResoraDialog>().isEmpty());
        };

        drive<OpenResoraDialog>([&](OpenResoraDialog *d) {  // failed: corrupted document
            d->setFile(p("bad.resora"));
            QVERIFY(d->startOpen());
            QTRY_VERIFY(!d->isRunning());
            QCOMPARE(d->state(), OpenResoraDialog::State::Failed);
            d->close();
        });
        fire(w, kOpen);
        unchanged();

        drive<OpenResoraDialog>([&](OpenResoraDialog *d) {  // cancelled while running
            d->setFile(p("c.resora"));
            QVERIFY(d->startOpen());
            d->close();  // cancels; the dialog closes once the worker is done
        });
        fire(w, kOpen);
        unchanged();

        QVERIFY(QFile::rename(p("pics/big.png"), p("moved.png")));  // relink failure: wrong content
        put(p("fake.bin"), QByteArray(5000, 'z'));
        drive<OpenResoraDialog>([&](OpenResoraDialog *d) {
            d->setFile(p("l.resora"));
            QVERIFY(d->startOpen());
            QTRY_VERIFY(!d->isRunning());
            QCOMPARE(d->state(), OpenResoraDialog::State::NeedsRelink);
            for (const auto &pr : d->relinkPanel()->model().unresolved())
                d->relinkPanel()->assign(pr.imageId, p("fake.bin"));
            QVERIFY(d->retryWithRelinks());
            QTRY_VERIFY(!d->isRunning());
            QCOMPARE(d->state(), OpenResoraDialog::State::NeedsRelink);
            QVERIFY(!d->session());
            d->close();
        });
        fire(w, kOpen);
        unchanged();
    }
};

QTEST_MAIN(TstMainWindow)
#include "tst_mainwindow.moc"
