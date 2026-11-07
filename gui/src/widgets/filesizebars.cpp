#include "filesizebars.h"

#include <QLocale>
#include <QPainter>

#include "backend/propertiesmodel.h"
#include "ui/theme.h"

namespace {
QString n(double v, int d)
{
    return QLocale(QLocale::English).toString(v, 'f', d);
}

// "1.50 KB" without the exact-bytes suffix of PropertiesModel::formatBytes.
QString shortBytes(qint64 b)
{
    return PropertiesModel::formatBytes(b).section(QStringLiteral("  ("), 0, 0);
}
}  // namespace

FileSizeBars::FileSizeBars(QWidget *parent) : InsightCanvas(tr("File size comparison"), 2 * kBarBlockHeight + 98, parent) {}

QString FileSizeBars::describe() const
{
    const auto text = [](const SideFacts &s) { return s.hasBytes ? shortBytes(s.bytes) : PropertiesModel::unavailableText(); };
    return tr("Image A: %1. Image B: %2.").arg(text(m_.a), text(m_.b));
}

void FileSizeBars::paintContent(QPainter &p, const QRect &area)
{
    qint64 maxBytes = 0;
    for (const SideFacts *s : {&m_.a, &m_.b})
        if (s->hasBytes)
            maxBytes = qMax(maxBytes, s->bytes);

    auto block = [&](const QRect &r, const SideFacts &s, const QColor &c, const QString &tag, Side me) {
        if (!s.hasBytes) {
            drawBarBlock(p, r, c, tag, s.name, PropertiesModel::unavailableText(), -1, tr("File size not available"), false);
            return;
        }
        const double frac = maxBytes > 0 ? double(s.bytes) / double(maxBytes) : 0.0;
        const QString sub = maxBytes > 0 ? tr("%1% of the larger file").arg(n(frac * 100.0, frac >= 0.995 ? 0 : 1)) : tr("Empty file (0 bytes)");
        drawBarBlock(p, r, c, tag, s.name, shortBytes(s.bytes), frac, sub, m_.largerFile == me || m_.largerFile == Side::Tie);
    };
    QRect r(area.left(), area.top() + 2, area.width(), kBarBlockHeight);
    block(r, m_.a, colorA(), QStringLiteral("A"), Side::A);
    r.translate(0, kBarBlockHeight + 6);
    block(r, m_.b, colorB(), QStringLiteral("B"), Side::B);

    const int y = r.bottom() + 12;
    p.setFont(Theme::display(10, QFont::DemiBold, 0.2));
    QString summary;
    if (!m_.sizesComparable) {
        p.setPen(Theme::mute());
        summary = tr("Difference unavailable: a file size is missing.");
    } else if (m_.largerFile == Side::Tie) {
        p.setPen(Theme::paper());
        summary = tr("Same file size  ·  0 B difference");
    } else {
        p.setPen(m_.largerFile == Side::A ? colorA() : colorB());
        summary = tr("%1 is larger:  +%2").arg(sideName(m_.largerFile), shortBytes(m_.byteDifference));
        if (m_.bytePercentValid)
            summary += tr("  ·  +%1%").arg(n(m_.bytePercent, m_.bytePercent >= 100 ? 0 : 1));
        else
            summary += tr("  ·  percentage not defined (the other file is 0 bytes)");
    }
    p.drawText(QRect(area.left(), y, area.width(), 36), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, summary);
    p.setPen(Theme::mute());
    p.setFont(Theme::display(8.5, QFont::Normal, 0.2));
    p.drawText(QRect(area.left(), y + 38, area.width(), 30), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               tr("Shared scale starting at 0. File size depends on format and compression, not only quality."));
}
