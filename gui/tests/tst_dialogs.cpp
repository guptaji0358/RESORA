#include <memory>

#include <QAbstractButton>
#include <QGuiApplication>
#include <QScreen>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "locations/locationstore.h"
#include "widgets/aboutdialog.h"
#include "widgets/chamferbutton.h"
#include "widgets/locationrow.h"
#include "widgets/locationsuggester.h"
#include "widgets/savedlocationspanel.h"
#include "widgets/framelessdialog.h"
#include "widgets/importdialog.h"
#include "widgets/settingsdialog.h"

// Window management for the shared dialog frame. Runs offscreen: geometry/close/Esc logic is
// verified here; real window dragging (startSystemMove) needs a real desktop.
class TstDialogs : public QObject
{
    Q_OBJECT
private:
    static QRect avail() { return QGuiApplication::primaryScreen()->availableGeometry(); }

    static void verifyOnScreen(QWidget &d)
    {
        const QRect g = d.frameGeometry();
        QVERIFY2(avail().contains(g), qPrintable(QStringLiteral("dialog %1 outside %2")
                                                     .arg(QDebug::toString(g), QDebug::toString(avail()))));
    }

    template <typename F> void checkAt(const QPoint &parentPos, F make)
    {
        QWidget host;
        host.resize(900, 640);
        host.move(parentPos);
        host.show();
        auto d = make(&host);
        d->show();
        QVERIFY(d->isVisible());
        verifyOnScreen(*d);
        QVERIFY(d->closeButton()->isVisible());
        QVERIFY(d->titleBar()->geometry().top() >= 0);
        QVERIFY(d->rect().contains(d->closeButton()->mapTo(d.get(), d->closeButton()->rect().bottomRight())));
    }

private slots:
    void settingsStaysOnScreenNearEdges()
    {
        QTemporaryDir dir;
        LocationStore store(dir.filePath("loc.json"));
        for (const QPoint &p : {QPoint(0, 0), QPoint(-300, -200), avail().bottomRight() - QPoint(200, 150)})
            checkAt(p, [&](QWidget *host) { return std::make_unique<SettingsDialog>(&store, host); });
    }

    void importAndAboutStayOnScreen()
    {
        for (const QPoint &p : {QPoint(0, 0), QPoint(-400, -300), avail().bottomRight() - QPoint(100, 100)}) {
            checkAt(p, [](QWidget *host) { return std::make_unique<ImportDialog>(QString(), host); });
            checkAt(p, [](QWidget *host) { return std::make_unique<AboutDialog>(host); });
        }
    }

    void withoutParentIsCentredOnScreen()
    {
        AboutDialog d;
        d.show();
        verifyOnScreen(d);
    }

    void isFramelessWithOwnCloseControl()
    {
        AboutDialog d;
        QVERIFY(d.windowFlags() & Qt::FramelessWindowHint);
        QVERIFY(d.closeButton());
    }

    void closeButtonRejects()
    {
        ImportDialog d{QString()};
        d.show();
        QSignalSpy spy(&d, &QDialog::rejected);
        QTest::mouseClick(d.closeButton(), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!d.isVisible());
    }

    void escapeCloses()
    {
        QTemporaryDir dir;
        LocationStore store(dir.filePath("loc.json"));
        SettingsDialog d(&store);
        d.show();
        QTest::keyClick(&d, Qt::Key_Escape);
        QVERIFY(!d.isVisible());
    }

    // ---- keyboard: accidental Enter must never activate a button ----------------------------------
    static QPushButton *buttonText(QWidget &root, const QString &text)
    {
        for (auto *b : root.findChildren<QPushButton *>())
            if (b->text() == text)
                return b;
        return nullptr;
    }
    static void press(QWidget &d, int key)
    {
        QTest::keyClick(d.focusWidget() ? d.focusWidget() : &d, Qt::Key(key));
    }

    void settingsOpensWithoutArmedButton()
    {
        QTemporaryDir dir;
        LocationStore store(dir.filePath("loc.json"));
        SettingsDialog d(&store);
        d.show();
        QVERIFY(QTest::qWaitForWindowExposed(&d));
        QVERIFY2(!qobject_cast<QAbstractButton *>(d.focusWidget()),
                 d.focusWidget() ? d.focusWidget()->metaObject()->className() : "");
        QPushButton *done = buttonText(d, "Done");
        QVERIFY(done);
        QVERIFY(!done->hasFocus());
        QVERIFY(!done->isDefault());
        for (auto *b : d.findChildren<QPushButton *>())
            QVERIFY2(!b->autoDefault() && !b->isDefault(), qPrintable(b->text()));
    }

    void enterWithoutFocusedActionDoesNotCloseSettings()
    {
        QTemporaryDir dir;
        LocationStore store(dir.filePath("loc.json"));
        for (int key : {Qt::Key_Return, Qt::Key_Enter}) {
            SettingsDialog d(&store);
            d.show();
            QSignalSpy spy(&d, &QDialog::finished);
            press(d, key);
            QVERIFY(d.isVisible());
            QCOMPARE(spy.count(), 0);
        }
    }

    void enterInSettingsTextFieldAddsButNeverCloses()
    {
        QTemporaryDir dir;
        LocationStore store(dir.filePath("loc.json"));
        QDir(dir.path()).mkpath("pics");
        for (int key : {Qt::Key_Return, Qt::Key_Enter}) {
            SettingsDialog d(&store);
            d.show();
            auto *panel = d.findChild<SavedLocationsPanel *>();
            QVERIFY(panel);
            const int before = panel->rowCount();
            panel->nameEdit()->setText(key == Qt::Key_Return ? "P1" : "P2");
            panel->pathEdit()->setText(dir.filePath("pics"));
            panel->nameEdit()->setFocus();
            QTest::keyClick(panel->nameEdit(), Qt::Key(key));
            QVERIFY(d.isVisible());
            QVERIFY(panel->rowCount() >= before);
        }
    }

    void clickingDoneStillCloses()
    {
        QTemporaryDir dir;
        LocationStore store(dir.filePath("loc.json"));
        SettingsDialog d(&store);
        d.show();
        QSignalSpy spy(&d, &QDialog::accepted);
        QTest::mouseClick(buttonText(d, "Done"), Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!d.isVisible());
    }

    void deliberatelyFocusedButtonActivates()
    {
        QWidget host;
        host.show();
        ChamferButton b("Go", &host);
        b.show();
        QVERIFY(QTest::qWaitForWindowActive(&host));
        int clicks = 0;
        QObject::connect(&b, &QPushButton::clicked, [&] { ++clicks; });
        int expected = 0;
        for (int key : {Qt::Key_Return, Qt::Key_Enter}) {
            b.clearFocus();
            b.setFocus(Qt::MouseFocusReason);  // focused by click/automatically: Enter ignored
            QTest::keyClick(&b, Qt::Key(key));
            QCOMPARE(clicks, expected);
            b.clearFocus();
            b.setFocus(Qt::TabFocusReason);    // reached with Tab: Enter activates
            QTest::keyClick(&b, Qt::Key(key));
            QCOMPARE(clicks, ++expected);
        }
        QCOMPARE(clicks, 2);
        QTest::keyClick(&b, Qt::Key_Space);    // Space always works
        QCOMPARE(clicks, 3);
    }

    void aboutAndMainHaveNoDefaultButton()
    {
        AboutDialog d;
        d.show();
        QVERIFY(!qobject_cast<QAbstractButton *>(d.focusWidget()));
        for (int key : {Qt::Key_Return, Qt::Key_Enter}) {
            press(d, key);
            QVERIFY(d.isVisible());
        }
        QTest::keyClick(&d, Qt::Key_Escape);
        QVERIFY(!d.isVisible());
    }

    void importDialogEnterBehaviour()
    {
        QTemporaryDir dir;
        // Enter with no focused action: nothing happens.
        for (int key : {Qt::Key_Return, Qt::Key_Enter}) {
            ImportDialog d{QString()};
            d.show();
            QVERIFY(!qobject_cast<QAbstractButton *>(d.focusWidget()));
            press(d, key);
            QVERIFY(d.isVisible());
            QVERIFY(!d.selection().isValid());
            for (auto *b : d.findChildren<QPushButton *>())
                QVERIFY2(!b->autoDefault() && !b->isDefault(), qPrintable(b->text()));
            QCOMPARE(d.findChildren<QShortcut *>().size(), 3);  // 1, 2, 3 tile shortcuts intact
        }
        // Enter in the path field imports that folder.
        for (int key : {Qt::Key_Return, Qt::Key_Enter}) {
            ImportDialog d{QString()};
            d.show();
            d.pathEdit()->setText(dir.path());
            d.pathEdit()->setFocus();
            QTest::keyClick(d.pathEdit(), Qt::Key(key));
            QVERIFY(!d.isVisible());
            QVERIFY(d.selection().isValid());
        }
    }

    void suggestionEnterSelectsHighlighted()
    {
        QTemporaryDir dir;
        LocationStore store(dir.filePath("loc.json"));
        QDir(dir.path()).mkpath("pics");
        QVERIFY(store.add("Pics", dir.filePath("pics")).ok());
        for (int key : {Qt::Key_Return, Qt::Key_Enter}) {
            ImportDialog d(QString(), nullptr, &store);
            d.show();
            auto *sug = d.pathEdit()->findChild<LocationSuggester *>();
            QVERIFY(sug);
            d.pathEdit()->setFocus();
            sug->showSuggestions();
            QVERIFY(sug->isPopupVisible());
            QTest::keyClick(d.pathEdit(), Qt::Key_Down);
            QTest::keyClick(d.pathEdit(), Qt::Key(key));
            QVERIFY(d.pathEdit()->text().contains("pics"));
            QVERIFY(!sug->isPopupVisible());
            QVERIFY(d.isVisible());                // choosing only fills the field
            QVERIFY(!d.selection().isValid());
        }
    }

    void inlineRenameEnterSavesEscapeCancels()
    {
        QTemporaryDir dir;
        LocationStore store(dir.filePath("loc.json"));
        QDir(dir.path()).mkpath("a");
        QVERIFY(store.add("Old", dir.filePath("a")).ok());
        int n = 0;
        for (int key : {Qt::Key_Return, Qt::Key_Enter}) {
            SettingsDialog d(&store);
            d.show();
            auto *panel = d.findChild<SavedLocationsPanel *>();
            LocationRow *row = panel->rowAt(0);
            row->beginRename();
            QLineEdit *edit = nullptr;
            for (auto *e : row->findChildren<QLineEdit *>())
                if (e->accessibleName() == "New location name")
                    edit = e;
            QVERIFY(edit && !edit->isHidden());
            // Escape cancels and keeps the old name.
            edit->setText("Discarded");
            QTest::keyClick(edit, Qt::Key_Escape);
            QCOMPARE(store.locations().first().name, QString(n ? "New1" : "Old"));
            QVERIFY(d.isVisible());  // Esc in the editor cancels the rename, not the dialog
            panel->rowAt(0)->beginRename();
            edit->setText(QString("New%1").arg(++n));
            QTest::keyClick(edit, Qt::Key(key));
            QCOMPARE(store.locations().first().name, QString("New%1").arg(n));
            QVERIFY(d.isVisible());  // Enter saves, never closes the dialog
        }
    }

    void reopeningRepositions()
    {
        QWidget host;
        host.resize(900, 640);
        host.move(0, 0);
        host.show();
        AboutDialog d(&host);
        d.show();
        d.move(-5000, -5000);  // user dragged it away
        d.hide();
        d.show();
        verifyOnScreen(d);
    }

    void tinyScreenAreaCapsSize()
    {
        ImportDialog d{QString()};
        d.show();
        QVERIFY(d.width() <= avail().width());
        QVERIFY(d.height() <= avail().height());
    }
};

QTEST_MAIN(TstDialogs)
#include "tst_dialogs.moc"
