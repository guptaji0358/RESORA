#include "brand.h"

#include <QPainter>
#include <QPixmap>

#include "ui/theme.h"

namespace {
struct Cell { int x, y, size; int tone; };  // 16x16 unit grid; tone 0..3 = paper alpha, 4 = ember

constexpr Cell kCells[] = {
    {0, 0, 8, 0},
    {8, 0, 4, 1},  {12, 0, 4, 2}, {8, 4, 4, 2},  {12, 4, 4, 3},
    {0, 8, 4, 1},  {4, 8, 4, 2},  {0, 12, 4, 2}, {4, 12, 4, 3},
    {8, 8, 4, 3},  {12, 8, 4, 3}, {8, 12, 4, 3},
    {12, 12, 2, 3}, {14, 12, 2, 3}, {12, 14, 2, 3}, {14, 14, 2, 4},
};
constexpr int kAlpha[] = {46, 96, 158, 230};
constexpr qreal kGap = 0.6;  // units between cells
}  // namespace

namespace Brand {

QString name() { return QStringLiteral("RESORA"); }
QString description() { return QStringLiteral("Image Resolution Analyzer"); }
QString tagline() { return QStringLiteral("See every pixel clearly."); }
QString version() { return QStringLiteral("0.1.0"); }

void paintMark(QPainter &p, const QRectF &target)
{
    const qreal side = qMin(target.width(), target.height());
    const qreal u = side / 16.0;
    const QPointF o(target.center().x() - side / 2, target.center().y() - side / 2);
    p.save();
    p.setPen(Qt::NoPen);
    for (const Cell &c : kCells) {
        QColor col = Theme::paper();
        if (c.tone == 4) col = Theme::ember();
        else col.setAlpha(kAlpha[c.tone]);
        p.setBrush(col);
        p.drawRect(QRectF(o.x() + (c.x + kGap / 2) * u, o.y() + (c.y + kGap / 2) * u,
                          (c.size - kGap) * u, (c.size - kGap) * u));
    }
    p.restore();
}

QIcon icon()
{
    QIcon ic;
    for (int s : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pm(s, s);
        pm.fill(Theme::ink());
        QPainter p(&pm);
        paintMark(p, QRectF(s * 0.12, s * 0.12, s * 0.76, s * 0.76));
        ic.addPixmap(pm);
    }
    return ic;
}

}  // namespace Brand
