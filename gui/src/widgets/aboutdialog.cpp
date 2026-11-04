#include "aboutdialog.h"

#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "ui/brand.h"
#include "ui/theme.h"

namespace {
class BigMark : public QWidget
{
public:
    explicit BigMark(QWidget *p) : QWidget(p) { setFixedSize(96, 96); }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        Brand::paintMark(p, rect());
    }
};
}  // namespace

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("About %1").arg(Brand::name()));
    setWindowIcon(Brand::icon());
    setModal(true);
    setStyleSheet(QStringLiteral("QDialog{background:#0D0C0B;}"));
    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(40, 32, 40, 28);
    col->setSpacing(10);

    col->addWidget(new BigMark(this), 0, Qt::AlignHCenter);
    auto *word = new QLabel(QStringLiteral("RES<span style='color:#FF5B2E'>ORA</span>"));
    word->setFont(Theme::display(26, QFont::Bold, 9));
    word->setStyleSheet(QStringLiteral("color:#EFE9DD;"));
    col->addWidget(word, 0, Qt::AlignHCenter);

    auto *desc = new QLabel(Brand::description().toUpper());
    desc->setProperty("role", QStringLiteral("caption"));
    col->addWidget(desc, 0, Qt::AlignHCenter);
    auto *tag = new QLabel(Brand::tagline());
    tag->setFont(Theme::display(11, QFont::Normal));
    tag->setStyleSheet(QStringLiteral("color:#EFE9DD;"));
    col->addSpacing(6);
    col->addWidget(tag, 0, Qt::AlignHCenter);
    auto *ver = new QLabel(tr("Version %1").arg(Brand::version()));
    ver->setFont(Theme::mono(8.5));
    ver->setStyleSheet(QStringLiteral("color:#9A9186;"));
    col->addWidget(ver, 0, Qt::AlignHCenter);
    auto *note = new QLabel(tr("Reads image headers only. Your originals are never modified."));
    note->setStyleSheet(QStringLiteral("color:#9A9186; font-size:8pt;"));
    col->addWidget(note, 0, Qt::AlignHCenter);

    auto *ok = new QPushButton(tr("Close"));
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    col->addSpacing(8);
    col->addWidget(ok, 0, Qt::AlignHCenter);
}
