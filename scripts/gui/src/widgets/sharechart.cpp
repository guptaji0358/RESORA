#include "widgets/sharechart.h"

#include <QPainter>

#include "backend/analyticsmodel.h"
#include "ui/theme.h"
#include "ui/uiformat.h"

namespace {
constexpr int kGlyphSlot = 52;
constexpr int kTextWidth = 190;
}  // namespace

ShareChart::ShareChart(const QString &accessibleName, QWidget *parent) : QWidget(parent)
{
    setAccessibleName(accessibleName);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

double ShareChart::fraction(double percent)
{
    return percent <= 0 ? 0.0 : qMin(1.0, percent / 100.0);
}

int ShareChart::rowHeight() const
{
    for (const Row &r : rows_)
        if (r.hasBytes)
            return 50;
    return 38;
}

QSize ShareChart::sizeHint() const
{
    return {420, qMax(1, int(rows_.size())) * rowHeight()};
}

QSize ShareChart::minimumSizeHint() const
{
    return {360, qMax(1, int(rows_.size())) * rowHeight()};
}

QString ShareChart::rowText(int i) const
{
    if (i < 0 || i >= rows_.size())
        return {};
    const Row &r = rows_[i];
    QString t = QObject::tr("%1: %2 images, %3").arg(r.name, UiFormat::count(r.count), AnalyticsModel::percentText(r.percent));
    if (r.hasBytes)
        t += QObject::tr(", %1 (%2 of storage)").arg(r.bytesText, AnalyticsModel::percentText(r.bytesPercent));
    return t;
}

void ShareChart::setRows(const QList<Row> &rows)
{
    rows_ = rows;
    QStringList lines;
    for (int i = 0; i < rows_.size(); ++i)
        lines << rowText(i);
    setAccessibleDescription(lines.join(QStringLiteral("; ")));
    setFixedHeight(sizeHint().height());
    updateGeometry();
    update();
}

void ShareChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const int rh = rowHeight();
    for (int i = 0; i < rows_.size(); ++i) {
        const Row &r = rows_[i];
        const bool zero = r.count == 0;
        const int top = i * rh;
        const QColor tone = zero ? Theme::mute() : QColor::fromHsv(14, qMax(60, 255 - i * 22), 255 - i * 6);

        // Glyph slot: a frame at the class's true aspect; dashed neutral frame when unknown.
        const QRect slot(0, top + 3, kGlyphSlot, rh - 8);
        p.setBrush(Qt::NoBrush);
        if (!r.frame.isEmpty()) {
            double w = qMax(16.0, slot.width() * r.glyphScale);
            double h = w * r.frame.height() / r.frame.width();
            if (h > slot.height()) {  // tall frames: fit the height instead
                h = slot.height();
                w = h * r.frame.width() / r.frame.height();
            }
            p.setPen(QPen(zero ? Theme::line() : tone, 1.5));
            p.setBrush(zero ? Qt::NoBrush : QBrush(QColor(tone.red(), tone.green(), tone.blue(), 40)));
            p.drawRect(QRectF(slot.left() + (slot.width() - w) / 2, slot.center().y() - h / 2, w, h));
        } else {
            p.setPen(QPen(Theme::mute(), 1, Qt::DashLine));
            p.drawRect(QRect(slot.left() + 12, slot.center().y() - 8, 28, 16));
        }

        const int x0 = kGlyphSlot + 12;
        const int barW = qMax(40, width() - x0 - kTextWidth);
        p.setPen(zero ? Theme::mute() : Theme::paper());
        p.setFont(Theme::display(11, QFont::DemiBold, 0.4));
        p.drawText(QRect(x0, top + 2, barW, 20), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(r.name, Qt::ElideRight, barW));

        const QRect track(x0, top + 26, barW, 3);
        p.fillRect(track, Theme::line());
        p.fillRect(QRect(track.left(), track.top(), int(track.width() * fraction(r.percent)), 3), tone);
        if (r.hasBytes) {
            const QRect t2(x0, top + 34, barW, 2);
            p.fillRect(t2, Theme::line());
            p.fillRect(QRect(t2.left(), t2.top(), int(t2.width() * fraction(r.bytesPercent)), 2), Theme::paper());
        }

        p.setPen(Theme::mute());
        p.setFont(Theme::mono(10));
        p.drawText(QRect(width() - kTextWidth, top, kTextWidth, 28), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("%1  ·  %2").arg(UiFormat::count(r.count), AnalyticsModel::percentText(r.percent)));
        if (r.hasBytes)
            p.drawText(QRect(width() - kTextWidth, top + 24, kTextWidth, 20), Qt::AlignRight | Qt::AlignVCenter,
                       QStringLiteral("%1  ·  %2").arg(r.bytesText, AnalyticsModel::percentText(r.bytesPercent)));
    }
}
