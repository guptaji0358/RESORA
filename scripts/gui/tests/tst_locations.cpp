#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QtTest>

#include "locations/locationrecommender.h"
#include "locations/locationstore.h"
#include "mainwindow.h"
#include "widgets/importdialog.h"
#include "widgets/locationrow.h"
#include "widgets/locationsuggester.h"
#include "widgets/reportselector.h"
#include "widgets/savedlocationspanel.h"

namespace {
QString mk(const QTemporaryDir &d, const QString &name)
{
    QDir(d.path()).mkpath(name);
    return d.filePath(name);
}
void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}
}  // namespace

class TstLocations : public QObject
{
    Q_OBJECT
    using R = LocationStore::Error;
private slots:
    void addRenameRemove()
    {
        QTemporaryDir d;
        LocationStore s(d.filePath("cfg/loc.json"));
        QString id;
        QVERIFY(s.add("  College   Images ", mk(d, "a"), &id).ok());
        QCOMPARE(s.locations().size(), 1);
        QCOMPARE(s.find(id).name, QString("College Images"));
        QVERIFY(s.rename(id, "Wallpapers").ok());
        QCOMPARE(s.find(id).name, QString("Wallpapers"));
        QVERIFY(s.setPath(id, mk(d, "b")).ok());
        QVERIFY(SavedLocations::samePath(s.find(id).path, QDir::fromNativeSeparators(d.filePath("b"))));
        QCOMPARE(s.rename("nope", "x").error, R::NotFound);
        QVERIFY(s.remove(id).ok());
        QVERIFY(s.locations().isEmpty());
        QCOMPARE(s.remove(id).error, R::NotFound);
    }

    void persistsAcrossReload()
    {
        QTemporaryDir d;
        const QString file = d.filePath("loc.json");
        {
            LocationStore s(file);
            QVERIFY(s.add("One", mk(d, "a")).ok());
            QVERIFY(s.add("Two", mk(d, "b")).ok());
        }
        LocationStore again(file);
        QCOMPARE(again.locations().size(), 2);
        QCOMPARE(again.locations()[1].name, QString("Two"));
        QVERIFY(again.warnings().isEmpty());
        QSignalSpy spy(&again, &LocationStore::changed);
        again.reload();
        QCOMPARE(spy.count(), 1);
    }

    void duplicatesAndValidation()
    {
        QTemporaryDir d;
        LocationStore s(d.filePath("loc.json"));
        const QString a = mk(d, "a"), b = mk(d, "b");
        QVERIFY(s.add("Pics", a).ok());
        QCOMPARE(s.add("pics", b).error, R::DuplicateName);
        QCOMPARE(s.add("Other", a + "/").error, R::DuplicatePath);
        QCOMPARE(s.add("Other", QDir::toNativeSeparators(a)).error, R::DuplicatePath);
        QCOMPARE(s.add("   ", b).error, R::EmptyName);
        QCOMPARE(s.add(QString(65, 'x'), b).error, R::NameTooLong);
        QCOMPARE(s.add("X", "").error, R::EmptyPath);
        QCOMPARE(s.add("X", "relative/dir").error, R::NotAbsolute);
        QCOMPARE(s.add("X", QString(5000, 'a').prepend(d.path() + "/")).error, R::PathTooLong);
        writeFile(d.filePath("file.txt"), "x");
        QCOMPARE(s.add("X", d.filePath("file.txt")).error, R::NotADirectory);
        QCOMPARE(s.locations().size(), 1);
        // renaming to its own name (different case) is allowed
        QVERIFY(s.rename(s.locations()[0].id, "PICS").ok());
    }

    void missingDirectoryIsKeptAndFlagged()
    {
        QTemporaryDir d;
        LocationStore s(d.filePath("loc.json"));
        const QString gone = d.filePath("gone");
        QVERIFY(s.add("Gone", gone).ok());  // unplugged drives / not-yet-created folders are allowed
        QCOMPARE(SavedLocations::probe(s.locations()[0].path), LocationStatus::Missing);
        LocationStore again(d.filePath("loc.json"));
        QCOMPARE(again.locations().size(), 1);  // never silently deleted
        QCOMPARE(SavedLocations::probe(QString()), LocationStatus::Invalid);
    }

    void malformedFileIsMovedAside()
    {
        QTemporaryDir d;
        const QString file = d.filePath("loc.json");
        writeFile(file, "{ not json");
        LocationStore s(file);
        QVERIFY(s.locations().isEmpty());
        QVERIFY(!s.warnings().isEmpty());
        QVERIFY(!s.isReadOnly());
        QVERIFY(!QDir(d.path()).entryList({"loc.json.corrupt-*"}).isEmpty());
        QVERIFY(s.add("Fresh", mk(d, "a")).ok());
    }

    void invalidEntriesArePreserved()
    {
        QTemporaryDir d;
        const QString file = d.filePath("loc.json");
        const QString good = QDir::fromNativeSeparators(mk(d, "a"));
        writeFile(file, QByteArray("{\"version\":1,\"locations\":[{\"name\":\"Good\",\"path\":\"" + good.toUtf8()
                                   + "\"},{\"name\":\"\",\"path\":\"x\"},42,{\"name\":\"Rel\",\"path\":\"rel\"}]}"));
        LocationStore s(file);
        QCOMPARE(s.locations().size(), 1);
        QVERIFY(!s.warnings().isEmpty());
        QVERIFY(s.add("New", mk(d, "b")).ok());
        QFile f(file);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray saved = f.readAll();
        QVERIFY(saved.contains("\"Rel\""));  // unreadable entries survive a save
        QVERIFY(saved.contains("42"));
    }

    void newerVersionIsReadOnly()
    {
        QTemporaryDir d;
        const QString file = d.filePath("loc.json");
        writeFile(file, "{\"version\":99,\"locations\":[]}");
        LocationStore s(file);
        QVERIFY(s.isReadOnly());
        QCOMPARE(s.add("X", mk(d, "a")).error, R::ReadOnly);
    }

    void saveFailureRollsBack()
    {
        QTemporaryDir d;
        writeFile(d.filePath("blocker"), "x");  // a file where the config directory should be
        LocationStore s(d.filePath("blocker/loc.json"));
        const auto r = s.add("X", mk(d, "a"));
        QCOMPARE(r.error, R::SaveFailed);
        QVERIFY(s.locations().isEmpty());
    }

    void recommendations()
    {
        QTemporaryDir d;
        LocationStore s(d.filePath("loc.json"));
        QVERIFY(s.add("College Images", mk(d, "college")).ok());
        QVERIFY(s.add("Wallpapers", mk(d, "walls")).ok());
        QVERIFY(s.add("Missing", d.filePath("gone")).ok());
        using namespace LocationRecommender;
        const Context folder{Kind::Folder, {}};
        QCOMPARE(recommend(s.locations(), "", folder).size(), 2);  // unavailable one is not offered
        QCOMPARE(recommend(s.locations(), "wall", folder).size(), 1);
        QCOMPARE(recommend(s.locations(), "zzz", folder).size(), 0);
        const Context csv{Kind::ReportFile, "csv"};
        const auto all = recommend(s.locations(), "myreport", csv);
        QCOMPARE(all.size(), 2);
        QVERIFY(all[0].value.endsWith("myreport.csv"));
        QVERIFY(recommend(s.locations(), "", csv)[0].value.endsWith("image_resolution_report.csv"));
        QVERIFY(recommend(s.locations(), "C:/elsewhere/x.csv", csv).isEmpty());
    }

    void reportSelectorIntegration()
    {
        QTemporaryDir d;
        LocationStore s(d.filePath("loc.json"));
        QVERIFY(s.add("Out", mk(d, "out")).ok());
        ReportSelector csv("CSV", "csv", "CSV files");
        QLineEdit unrelated;
        csv.setLocationStore(&s);
        csv.setWanted(true);
        csv.show();
        csv.pathEdit()->setText("results.csv");
        auto *sug = csv.pathEdit()->findChild<LocationSuggester *>();
        QVERIFY(sug);
        sug->showSuggestions();
        QVERIFY(sug->isPopupVisible());
        QCOMPARE(sug->rowCount(), 1);
        sug->choose(0);
        QVERIFY(!sug->isPopupVisible());
        QVERIFY(csv.path().endsWith("out/results.csv") || csv.path().endsWith("out\\results.csv"));
        QVERIFY(QFileInfo(csv.path()).dir().exists());
        QVERIFY(!QFileInfo::exists(csv.path()));  // choosing never writes
        QVERIFY(unrelated.findChild<LocationSuggester *>() == nullptr);
    }

    void importDialogFolderField()
    {
        QTemporaryDir d;
        LocationStore s(d.filePath("loc.json"));
        QVERIFY(s.add("Pics", mk(d, "pics")).ok());
        ImportDialog dlg(QString(), nullptr, &s);
        dlg.show();
        auto *sug = dlg.pathEdit()->findChild<LocationSuggester *>();
        QVERIFY(sug);
        sug->showSuggestions();
        QCOMPARE(sug->rowCount(), 1);
        sug->choose(0);
        QVERIFY(dlg.pathEdit()->text().contains("pics"));
        QVERIFY(!dlg.selection().isValid());  // populating the field imports nothing
    }

    void settingsPanel()
    {
        QTemporaryDir d;
        LocationStore s(d.filePath("loc.json"));
        SavedLocationsPanel p(&s);
        QVERIFY(p.addLocation("A", mk(d, "a")).ok());
        QCOMPARE(p.rowCount(), 1);
        QVERIFY(!p.addLocation("A", mk(d, "b")).ok());
        QVERIFY(!p.message().isEmpty());
        QVERIFY(p.addLocation("Gone", d.filePath("gone")).ok());
        QCOMPARE(p.rowCount(), 2);
        QCOMPARE(p.rowAt(0)->status(), LocationStatus::Available);
        QCOMPARE(p.rowAt(1)->status(), LocationStatus::Missing);
        QVERIFY(!p.rowAt(1)->statusMessage().isEmpty());
        QCOMPARE(p.rowAt(1)->fullPath(), QDir::toNativeSeparators(QDir::fromNativeSeparators(d.filePath("gone"))));
        const QString id = p.rowAt(1)->locationId();
        QVERIFY(p.changeLocationPath(id, mk(d, "moved")).ok());
        QCOMPARE(p.rowAt(1)->status(), LocationStatus::Available);
        QVERIFY(p.renameLocation(id, "Moved").ok());
        QVERIFY(p.removeLocation(id).ok());
        QCOMPARE(p.rowCount(), 1);
    }

    void mainWindowWiring()
    {
        QTemporaryDir d;
        MainWindow w(nullptr, d.filePath("loc.json"));
        QVERIFY(w.locationStore()->add("Out", mk(d, "out")).ok());
        w.csvSelector()->setWanted(true);
        w.txtSelector()->setWanted(true);
        QVERIFY(w.csvSelector()->pathEdit()->findChild<LocationSuggester *>());
        QVERIFY(w.txtSelector()->pathEdit()->findChild<LocationSuggester *>());
        QVERIFY(!w.isScanning());
    }
};

QTEST_MAIN(TstLocations)
#include "tst_locations.moc"
