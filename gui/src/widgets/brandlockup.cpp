#include "widgets/brandlockup.h"

#include <QFontMetrics>
#include <QPainter>

#include "ui/brand.h"
#include "ui/theme.h"

namespace {
constexpr int kMark = 28;
constexpr int kGapMark = 12;
constexpr int kGapDivider = 14;
QFont wordFont() { return Theme::display(14, QFont::Bold, 5.0); }
QFont descFont() { return Theme::display(7.5, QFont::DemiBold, 2.5); }
}  // namespace

BrandLockup::BrandLockup(QWidget *parent) : QWidget(parent)
{
    setAccessibleName(Brand::name() + QStringLiteral(" - ") + Brand::description());
    setToolTip(Brand::tagline() + QStringLiteral("  (F1: About)"));
}

QSize BrandLockup::sizeHint() const
{
    const int word = QFontMetrics(wordFont()).horizontalAdvance(Brand::name());
    const int desc = QFontMetrics(descFont()).horizontalAdvance(Brand::description().toUpper());
    return {kMark + kGapMark + word + kGapDivider * 2 + 1 + desc, 36};
}

void BrandLockup::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    Brand::paintMark(p, QRectF(0, (height() - kMark) / 2.0, kMark, kMark));

    const QFont wf = wordFont();
    const QFontMetrics fm(wf);
    int x = kMark + kGapMark;
    const int base = (height() + fm.ascent() - fm.descent()) / 2;
    p.setFont(wf);
    p.setPen(Theme::paper());
    p.drawText(x, base, QStringLiteral("RES"));
    x += fm.horizontalAdvance(QStringLiteral("RES"));
    p.setPen(Theme::ember());
    p.drawText(x, base, QStringLiteral("ORA"));
    x += fm.horizontalAdvance(QStringLiteral("ORA")) + kGapDivider;

    p.setPen(Theme::lineHi());
    p.drawLine(x, height() / 2 - 8, x, height() / 2 + 8);
    x += 1 + kGapDivider;
    const QFont df = descFont();
    const QFontMetrics dm(df);
    p.setFont(df);
    p.setPen(Theme::mute());
    p.drawText(x, (height() + dm.ascent() - dm.descent()) / 2, Brand::description().toUpper());
}
