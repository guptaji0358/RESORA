#include "settingsdialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "chamferbutton.h"
#include "savedlocationspanel.h"
#include "ui/icons.h"
#include "ui/theme.h"

SettingsDialog::SettingsDialog(LocationStore *store, QWidget *parent) : FramelessDialog(tr("Settings"), parent)
{
    setObjectName(QStringLiteral("settingsDialog"));
    setMinimumSize(640, 600);  // capped to the screen in FramelessDialog::placeOnScreen

    auto *eyebrow = new QLabel(tr("SETTINGS  /  SAVED LOCATIONS"));
    eyebrow->setProperty("role", QStringLiteral("eyebrow"));
    auto *head = new QLabel(tr("Folders you use often."));
    head->setFont(Theme::display(20, QFont::DemiBold));
    auto *lede = new QLabel(tr("RESORA offers these when you pick a source folder or a report destination. "
                               "They are shortcuts only: nothing is scanned or written until you start a scan."));
    lede->setProperty("role", QStringLiteral("muted"));
    lede->setWordWrap(true);
    panel_ = new SavedLocationsPanel(store);

    auto *close = new ChamferButton(tr("Done"));
    close->setProperty("variant", QStringLiteral("primary"));
    close->setAutoDefault(false);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    auto *col = bodyLayout();
    auto *mark = new QLabel;
    mark->setFixedSize(20, 20);
    mark->setPixmap(Icons::pixmap(QStringLiteral("bookmark"), 20, Theme::paper(), Theme::ember(), devicePixelRatioF()));
    auto *eyebrowRow = new QHBoxLayout;
    eyebrowRow->setSpacing(10);
    eyebrowRow->addWidget(mark);
    eyebrowRow->addWidget(eyebrow, 1);
    col->addLayout(eyebrowRow);
    col->addWidget(head);
    col->addWidget(lede);
    col->addSpacing(8);
    col->addWidget(panel_, 1);
    col->addWidget(close, 0, Qt::AlignRight);
}
