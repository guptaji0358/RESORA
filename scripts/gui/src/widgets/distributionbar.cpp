#include "widgets/distributionbar.h"

#include <QPainter>
#include <algorithm>
#include <cmath>
#include <QVBoxLayout>

#include "ui/theme.h"
#include "ui/uiformat.h"

namespace {
struct Std { const char *key; int w, h; };
// Known standards (substring match on the backend label) -> true pixel size for the glyph.
const Std kStd[] = {{"8K", 7680, 4320}, {"5K", 5120, 2880}, {"DCI 4K", 4096, 2160}, {"UHD 4K", 3840, 2160},
                    {"QHD", 2560, 1440}, {"DCI 2K", 2048, 1080}, {"Full HD", 1920, 1080}, {"HD", 1280, 720}};
QSize glyphSize(const QString &label)
{
    if (label.startsWith(QLatin1String("Custom")) || label.startsWith(QLatin1String("Panorama"))
        || label.startsWith(QLatin1String("Above")) || label.startsWith(QLatin1String("Below")))
        return {0, 0};   // not a standard: never given a standard's frame
    for (const Std &s : kStd)
        if (label.contains(QLatin1String(s.key)))
            return {s.w, s.h};
    return {0, 0};   // custom / panorama / unknown: drawn as a dashed neutral frame
}
}  // namespace

// One row: a frame glyph whose area follows the real pixel size, name, share bar, count.
class DistributionBar::Row : public QWidget
{
public:
    Row(const Entry &e, qint64 total, int rank) : e_(e), total_(total), rank_(rank)
    {
        setFixedHeight(40);
        setAccessibleName(QStringLiteral("%1: %2").arg(e.first, UiFormat::count(e.second)));
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const QColor tone = QColor::fromHsv(14, qMax(60, 255 - rank_ * 28), 255 - rank_ * 8);
        const QSize sz = glyphSize(e_.first);
        const QRect slot(0, 2, 60, 36);
        if (sz.width() > 0) {   // linear size relative to 8K, width capped to the slot
            const double k = std::pow(double(sz.width()) / 7680.0, 0.55);   // compressed so HD stays readable
            const int gw = qMax(16, int(slot.width() * k)), gh = qMax(9, gw * sz.height() / sz.width());
            p.setPen(QPen(tone, 1.5));
            p.setBrush(QColor(tone.red(), tone.green(), tone.blue(), 40));
            p.drawRect(QRect(slot.left(), slot.center().y() - gh / 2, gw, gh));
        } else {
            p.setPen(QPen(Theme::mute(), 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRect(slot.left(), slot.center().y() - 8, 28, 16));
        }
        const int x0 = 72, numW = 150;
        p.setPen(Theme::paper());
        p.setFont(Theme::display(11, QFont::DemiBold, 0.4));
        p.drawText(QRect(x0, 2, width() - x0 - numW, 20), Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(e_.first, Qt::ElideRight, width() - x0 - numW));
        const QRect track(x0, 28, width() - x0 - numW, 3);
        p.fillRect(track, Theme::line());
        p.fillRect(QRect(track.left(), track.top(), int(track.width() * share(e_.second, total_)), 3), tone);
        p.setPen(Theme::mute());
        p.setFont(Theme::mono(10));
        p.drawText(QRect(width() - numW, 0, numW, 30), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("%1  %2%").arg(UiFormat::count(e_.second))
                       .arg(share(e_.second, total_) * 100, 0, 'f', 1));
    }
private:
    Entry e_;
    qint64 total_;
    int rank_;
};

QSize DistributionBar::standardSize(const QString &label)
{
    return glyphSize(label);
}

DistributionBar::DistributionBar(QWidget *parent) : QWidget(parent), legend_(new QVBoxLayout(this))
{
    setAccessibleName(tr("Resolution distribution"));
    legend_->setContentsMargins(0, 0, 0, 0);
    legend_->setSpacing(2);
}

double DistributionBar::share(qint64 count, qint64 total)
{
    return total <= 0 ? 0.0 : qBound(0.0, double(count) / double(total), 1.0);
}

void DistributionBar::setEntries(const QList<Entry> &in)
{
    while (QLayoutItem *it = legend_->takeAt(0)) {
        if (QWidget *w = it->widget())
            w->deleteLater();
        delete it;
    }
    QList<Entry> kept;
    qint64 total = 0;
    for (const Entry &e : in)
        if (e.second > 0) {
            kept << e;
            total += e.second;
        }
    // A true ladder: known standards by pixel width ascending, unmatched classes last.
    std::stable_sort(kept.begin(), kept.end(), [](const Entry &a, const Entry &b) {
        const int wa = glyphSize(a.first).width(), wb = glyphSize(b.first).width();
        return (wa ? wa : 1 << 30) < (wb ? wb : 1 << 30);
    });
    rows_ = kept.size();
    for (int i = 0; i < rows_; ++i)
        legend_->addWidget(new Row(kept[i], total, i));
}
