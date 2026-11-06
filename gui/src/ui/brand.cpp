#include "brand.h"

#include <QPainter>
#include <QPixmap>

#include "ui/icons.h"
#include "ui/theme.h"

namespace Brand {

QString name() { return QStringLiteral("RESORA"); }
QString description() { return QStringLiteral("Image Resolution Analyzer"); }
QString tagline() { return QStringLiteral("See every pixel clearly."); }
QString version() { return QStringLiteral("0.1.0"); }

void paintMark(QPainter &p, const QRectF &target)
{
    const qreal side = qMin(target.width(), target.height());
    const qreal dpr = p.device()->devicePixelRatioF();
    const int px = qMax(1, qRound(side));
    p.drawPixmap(QPointF(target.center().x() - px / 2.0, target.center().y() - px / 2.0),
                 Icons::artwork(QStringLiteral("branding/mark"), QSize(px, px), dpr));
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
