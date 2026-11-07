#include "widgets/resolutionchart.h"

#include <QLocale>
#include <QPainter>

#include "ui/theme.h"

namespace {
QString n(double v, int d)
{
    return QLocale(QLocale::English).toString(v, 'f', d);
}
}  // namespace

ResolutionChart::ResolutionChart(QWidget *parent) : InsightCanvas(tr("Resolution comparison"), 2 * kBarBlockHeight + 96, parent) {}

QString ResolutionChart::missingText() const
{
    QStringList bad;
    if (!m_.a.hasSize)
        bad << tr("Image A");
    if (!m_.b.hasSize)
        bad << tr("Image B");
    return tr("Resolution unavailable: dimensions are missing or invalid for %1.").arg(bad.join(tr(" and ")));
}

QString ResolutionChart::describe() const
{
    if (m_.morePixels == Side::Tie)
        return tr("Both images have %1 megapixels (%2).").arg(n(m_.a.mp, 2), InsightMetricsBuilder::sizeText(m_.a.w, m_.a.h));
    return tr("Image A: %1 MP (%2). Image B: %3 MP (%4). %5 has %6 the pixels.")
        .arg(n(m_.a.mp, 2), InsightMetricsBuilder::sizeText(m_.a.w, m_.a.h), n(m_.b.mp, 2), InsightMetricsBuilder::sizeText(m_.b.w, m_.b.h),
             sideName(m_.morePixels), InsightMetricsBuilder::multiplier(m_.pixelRatio));
}

void ResolutionChart::paintContent(QPainter &p, const QRect &area)
{
    const qint64 maxPx = qMax(m_.a.pixels(), m_.b.pixels());
    auto block = [&](const QRect &r, const SideFacts &s, const QColor &c, const QString &tag, Side me) {
        const double frac = double(s.pixels()) / double(maxPx);
        const QString sub = tr("%1 px  ·  %2% of the larger").arg(InsightMetricsBuilder::sizeText(s.w, s.h), n(frac * 100.0, frac >= 0.995 ? 0 : 1));
        drawBarBlock(p, r, c, tag, s.name, tr("%1 MP").arg(n(s.mp, 2)), frac, sub, m_.morePixels == me || m_.morePixels == Side::Tie);
    };
    QRect r(area.left(), area.top() + 2, area.width(), kBarBlockHeight);
    block(r, m_.a, colorA(), QStringLiteral("A"), Side::A);
    r.translate(0, kBarBlockHeight + 6);
    block(r, m_.b, colorB(), QStringLiteral("B"), Side::B);

    // Summary: which image has more pixels, and by how much (absolute and relative).
    const int y = r.bottom() + 12;
    p.setFont(Theme::display(10, QFont::DemiBold, 0.2));
    QString summary;
    if (m_.morePixels == Side::Tie) {
        p.setPen(Theme::paper());
        summary = tr("Same pixel count  ·  0.00 MP difference");
    } else {
        p.setPen(m_.morePixels == Side::A ? colorA() : colorB());
        summary = tr("%1 has more pixels:  +%2 MP  ·  +%3%  ·  %4 the pixel count")
                      .arg(sideName(m_.morePixels), n(m_.mpDifference, 2), n(m_.pixelPercent, m_.pixelPercent >= 100 ? 0 : 1),
                           InsightMetricsBuilder::multiplier(m_.pixelRatio));
    }
    const QRect sr(area.left(), y, area.width(), 36);
    p.drawText(sr, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, summary);
    p.setPen(Theme::mute());
    p.setFont(Theme::display(8.5, QFont::Normal, 0.2));
    p.drawText(QRect(area.left(), y + 38, area.width(), 30), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               tr("Both bars use one scale starting at 0. More pixels does not mean better quality."));
}
