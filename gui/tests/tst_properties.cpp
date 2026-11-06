#include <QImage>
#include <QJsonDocument>
#include <QLabel>
#include <QTemporaryDir>
#include <QtTest>

#include "backend/propertiesmodel.h"
#include "widgets/propertiesdialog.h"

namespace {
QJsonObject parse(const char *json)
{
    return QJsonDocument::fromJson(json).object();
}

const PropertyRow *find(const ImageProperties &p, const QString &label)
{
    for (const PropertySection &s : p.sections)
        for (const PropertyRow &r : s.rows)
            if (r.label == label)
                return &r;
    return nullptr;
}

// A readable image with no optional metadata at all.
const char *kBare = R"({"path":"C:/x/a.png","name":"a.png","status":"ok","error":null,"file_size":1536,
 "format":"PNG","warnings":[],"layout":"landscape","aspect_ratio":1.778,"aspect_label":"16:9","megapixels":2.07,
 "resolution":"Full HD","color":{"mode":"RGB","mode_name":"RGB","channels":3,"bits_per_channel":8,"has_alpha":false,"icc_profile":null},
 "dpi":null,"exif":{"available":false,"fields":[],"gps_present":false,"orientation":null},"frames":null,
 "dimensions":{"stored_width":1920,"stored_height":1080,"width":1920,"height":1080,"orientation_tag":null,"orientation_label":null,"exif_rotated":false}})";
}  // namespace

class TstProperties : public QObject
{
    Q_OBJECT
private slots:
    void formatsBytesAndDimensions()
    {
        QCOMPARE(PropertiesModel::formatBytes(0), QStringLiteral("0 bytes"));
        QCOMPARE(PropertiesModel::formatBytes(1536), QStringLiteral("1.50 KB  (1,536 bytes)"));
        QCOMPARE(PropertiesModel::formatDimensions(1920, 1080), QStringLiteral("1,920 × 1,080 px"));
    }

    void missingMetadataIsUnavailableNotZero()
    {
        const ImageProperties p = PropertiesModel::fromJson(parse(kBare));
        QVERIFY(p.ok());
        for (const QString &label : {QStringLiteral("DPI / PPI"), QStringLiteral("Color profile"),
                                     QStringLiteral("EXIF metadata"), QStringLiteral("EXIF orientation")}) {
            const PropertyRow *r = find(p, label);
            QVERIFY2(r, qPrintable(label));
            QVERIFY2(!r->available, qPrintable(label));
            QCOMPARE(r->value, PropertiesModel::unavailableText());
        }
        QCOMPARE(find(p, QStringLiteral("Megapixels"))->value, QStringLiteral("2.07 MP"));
        QCOMPARE(find(p, QStringLiteral("Aspect ratio"))->value, QStringLiteral("16:9  (1.778)"));
        QCOMPARE(find(p, QStringLiteral("File size"))->value, QStringLiteral("1.50 KB  (1,536 bytes)"));
        QVERIFY(!find(p, QStringLiteral("Stored size")));  // only shown when EXIF rotation changed the size
    }

    void presentMetadataIsShown()
    {
        QJsonObject o = parse(kBare);
        o["dpi"] = parse(R"({"x":300,"y":300,"source":"file header"})");
        o["dimensions"] = parse(R"({"stored_width":400,"stored_height":200,"width":200,"height":400,
            "orientation_tag":6,"orientation_label":"Rotated 90° clockwise","exif_rotated":true})");
        const ImageProperties p = PropertiesModel::fromJson(o);
        QCOMPARE(find(p, QStringLiteral("DPI / PPI"))->value, QStringLiteral("300"));
        QVERIFY(find(p, QStringLiteral("DPI / PPI"))->available);
        QCOMPARE(find(p, QStringLiteral("Stored size"))->value, QStringLiteral("400 × 200 px"));
        QVERIFY(find(p, QStringLiteral("EXIF orientation"))->value.contains(QStringLiteral("tag 6")));
    }

    void failedImageHasReasonAndNoSections()
    {
        const ImageProperties p = PropertiesModel::fromJson(
            parse(R"({"path":"C:/x/b.jpg","name":"b.jpg","status":"corrupted","error":"not a valid image","file_size":4,"warnings":[]})"));
        QVERIFY(!p.ok());
        QVERIFY(p.sections.isEmpty());
        QVERIFY(p.toPlainText().contains(QStringLiteral("not a valid image")));
    }

    void dialogShowsParsedImages()
    {
        PropertiesDialog d(QStringList{});
        QCOMPARE(d.imageCount(), 0);
        d.showImages({PropertiesModel::fromJson(parse(kBare)), PropertiesModel::fromJson(parse(kBare))});
        QCOMPARE(d.imageCount(), 2);
        QCOMPARE(d.currentIndex(), 0);
        QVERIFY(d.currentText().contains(QStringLiteral("1,920 × 1,080 px")));
        QVERIFY(d.findChildren<QLabel *>(QStringLiteral("propUnavailable")).size() > 0);
    }

    // End to end through the real backend process (needs the engine + venv, like tst_backendprocess).
    void dialogLoadsPropertiesFromBackend()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("probe.png"));
        QImage img(64, 32, QImage::Format_RGB32);
        img.fill(Qt::red);
        QVERIFY(img.save(path));
        QFile orig(path);
        QVERIFY(orig.open(QIODevice::ReadOnly));
        const QByteArray before = orig.readAll();
        orig.close();

        PropertiesDialog d(QStringList{path});
        QTRY_VERIFY_WITH_TIMEOUT(!d.isLoading(), 20000);
        QCOMPARE(d.imageCount(), 1);
        QVERIFY2(d.currentText().contains(QStringLiteral("64 × 32 px")), qPrintable(d.currentText()));
        QVERIFY(d.currentText().contains(QStringLiteral("PNG")));

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), before);  // original untouched
    }
};

QTEST_MAIN(TstProperties)
#include "tst_properties.moc"
