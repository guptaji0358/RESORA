#include <QJsonDocument>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

#include "backend/duplicatesmodel.h"
#include "widgets/duplicatefinder.h"

namespace {
// Shape documented in docs/DUPLICATES.md: 1 exact group (3 files) + 1 similar group (2 images).
const char *kResult = R"JSON({"stats":{"total_files":10,"empty_files":0,"exact_groups":1,"exact_files":3,"exact_extra_files":2,
 "exact_bytes":3000,"exact_savings_bytes":2000,"similar_groups":1,"similar_files":2,"similar_bytes":900,"skipped":1,
 "cache_hits":0,"flat_images":0},
 "settings":{"threshold":90,"similar":true},
 "exact_groups":[{"id":"e1","size":1000,"count":3,"total_bytes":3000,"savings_bytes":2000,"files":[
  {"path":"C:/a/1.png","name":"1.png","folder":"C:/a","format":"PNG","width":10,"height":10,"file_size":1000},
  {"path":"C:/a/2.png","name":"2.png","folder":"C:/a","format":"PNG","width":10,"height":10,"file_size":1000},
  {"path":"C:/b/3.png","name":"3.png","folder":"C:/b","format":"PNG","width":10,"height":10,"file_size":1000}]}],
 "similar_groups":[{"id":"s1","count":2,"category":"Very similar","lowest_similarity":95.3,"total_bytes":900,"files":[
  {"path":"C:/a/x.png","name":"x.png","format":"PNG","width":20,"height":20,"file_size":500,"similarity":100.0,"category":"Nearly identical","anchor":true,"identical_copies":0},
  {"path":"C:/a/y.jpg","name":"y.jpg","format":"JPEG","width":10,"height":10,"file_size":400,"similarity":95.3,"category":"Very similar","anchor":false,"identical_copies":0}]}],
 "skipped":[{"path":"C:/a/bad.png","status":"corrupted","error":"x"}],"skipped_truncated":false})JSON";

QJsonObject sample() { return QJsonDocument::fromJson(kResult).object(); }
}  // namespace

class TstDuplicates : public QObject
{
    Q_OBJECT
private slots:
    void parsesExactAndSimilarSeparately()
    {
        const DupResult r = DuplicatesModel::fromJson(sample());
        QVERIFY(r.valid);
        QCOMPARE(r.exact.size(), 1);
        QCOMPARE(r.similar.size(), 1);
        QVERIFY(r.exact.first().exact);
        QVERIFY(!r.similar.first().exact);
        QCOMPARE(r.stats.exactSavings, qint64(2000));
        QCOMPARE(r.stats.similarBytes, qint64(900));
        QCOMPARE(r.exact.first().files.first().similarity, -1.0);  // no score on exact matches
        QCOMPARE(r.similar.first().files.last().similarity, 95.3);
        QCOMPARE(r.skipped.size(), 1);
        QVERIFY(!DuplicatesModel::fromJson(QJsonObject()).valid);
    }

    void requestParams()
    {
        DupRequest q;
        q.folders = {"C:/p"};
        q.exclude = {"C:/p/skip"};
        q.threshold = 95;
        q.cachePath = "C:/c.sqlite";
        const QJsonObject o = QJsonDocument::fromJson(q.paramsJson()).object();
        QCOMPARE(o["folders"].toArray().first().toString(), QString("C:/p"));
        QCOMPARE(o["threshold"].toDouble(), 95.0);
        QCOMPARE(o["exclude"].toArray().size(), 1);
        QVERIFY(!o.contains("files"));
        QCOMPARE(o["cache_path"].toString(), QString("C:/c.sqlite"));
    }

    void formatting()
    {
        QCOMPARE(DuplicatesModel::similarityText(96.94), QString("96.9%"));
        QCOMPARE(DuplicatesModel::similarityText(-1), QString("—"));
        QCOMPARE(DuplicatesModel::bytesText(-1), QString("Not available"));
    }

    void dialogShowsGroupsAndNeverDeletes()
    {
        DuplicateFinder dlg({}, QString(), QString());
        QVERIFY(dlg.validationError().length() > 0);  // nothing chosen, nothing scanned
        dlg.showResult(DuplicatesModel::fromJson(sample()));
        QCOMPARE(dlg.groupCount(), 1);
        QCOMPARE(dlg.memberRowCount(), 3);
        dlg.showKind(false);
        QCOMPARE(dlg.groupCount(), 1);
        QCOMPARE(dlg.memberRowCount(), 2);
        // No control that removes files exists anywhere in the dialog.
        for (QPushButton *b : dlg.findChildren<QPushButton *>()) {
            const QString t = b->text().toLower();
            QVERIFY2(!t.contains("delete") && !t.contains("trash") && !t.contains("recycle")
                     && !t.contains("move to"), qPrintable(t));  // "Remove" only edits the scan-source list
        }
    }

    void compareSignal()
    {
        DuplicateFinder dlg({}, QString(), QString());
        dlg.showResult(DuplicatesModel::fromJson(sample()));
        QSignalSpy spy(&dlg, &DuplicateFinder::compareRequested);
        QPushButton *cmp = nullptr;
        for (QPushButton *b : dlg.findChildren<QPushButton *>())
            if (b->text() == "Compare" && b->isEnabled()) {
                cmp = b;
                break;
            }
        QVERIFY(cmp);
        cmp->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QString("C:/a/1.png"));
        QCOMPARE(spy.first().at(1).toString(), QString("C:/a/2.png"));
    }

    void sourceSelection()
    {
        const QStringList coll = {"C:/x/a.png", "C:/x/b.png"};
        DuplicateFinder dlg(coll, QString(), QString());
        dlg.addExclusion("C:/x/skip");
        dlg.setUseCollection(true);
        DupRequest r = dlg.request();
        QCOMPARE(r.files, coll);
        QCOMPARE(r.exclude.size(), 1);
        QVERIFY(dlg.validationError().isEmpty());
        DuplicateFinder none({}, QString(), QString());
        none.setUseCollection(true);  // no collection: stays off
        QVERIFY(none.request().files.isEmpty());
    }
};

QTEST_MAIN(TstDuplicates)
#include "tst_duplicates.moc"
