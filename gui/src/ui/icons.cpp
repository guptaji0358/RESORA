#include "icons.h"

#include <QFile>
#include <QIconEngine>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>
#include <QtGlobal>

#include "ui/theme.h"

namespace {
const char kMainToken[] = "#EFE9DD";
const char kAccentToken[] = "#FF5B2E";

QByteArray load(const QString &name)
{
    QFile f(Icons::resourcePath(name));
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("RESORA: missing icon asset '%s'", qPrintable(name));
        return {};
    }
    return f.readAll();
}

QPixmap brokenGlyph(const QSize &box, qreal dpr)
{
    QPixmap pm((box * dpr).expandedTo({1, 1}));
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(dpr);
    QPainter p(&pm);
    p.setPen(QPen(Theme::alert(), 1.5, Qt::DashLine));
    const QRectF r = QRectF(QPointF(0, 0), QSizeF(box)).adjusted(1.5, 1.5, -1.5, -1.5);
    p.drawRect(r);
    p.drawLine(r.topLeft(), r.bottomRight());
    return pm;
}

QPixmap render(const QString &name, const QSize &box, const QColor *main, const QColor *accent, qreal dpr)
{
    const QString key = QStringLiteral("resora/%1/%2x%3@%4/%5/%6")
                            .arg(name).arg(box.width()).arg(box.height()).arg(dpr)
                            .arg(main ? main->name(QColor::HexArgb) : QString(), accent ? accent->name(QColor::HexArgb) : QString());
    QPixmap cached;
    if (QPixmapCache::find(key, &cached))
        return cached;

    QByteArray data = load(name);
    if (main)
        data.replace(kMainToken, main->name(QColor::HexRgb).toLatin1());
    if (accent)
        data.replace(kAccentToken, accent->name(QColor::HexRgb).toLatin1());
    QSvgRenderer renderer(data);
    if (data.isEmpty() || !renderer.isValid()) {
        if (!data.isEmpty())
            qWarning("RESORA: icon asset '%s' is not a valid SVG", qPrintable(name));
        return brokenGlyph(box, dpr);
    }
    // Fit the SVG's aspect ratio inside the box, centred.
    const QSizeF vb = renderer.defaultSize();
    QSizeF fit = vb.scaled(QSizeF(box), Qt::KeepAspectRatio);
    QPixmap pm((box * dpr).expandedTo({1, 1}));
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(dpr);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    renderer.render(&p, QRectF((box.width() - fit.width()) / 2, (box.height() - fit.height()) / 2, fit.width(), fit.height()));
    p.end();
    QPixmapCache::insert(key, pm);
    return pm;
}

// QIconEngine that renders on demand for the exact size and device pixel ratio requested.
class SvgIconEngine : public QIconEngine
{
public:
    explicit SvgIconEngine(const QString &name) : name_(name) {}
    QIconEngine *clone() const override { return new SvgIconEngine(name_); }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State) override
    {
        return scaledPixmap(size, mode, QIcon::Off, 1.0);
    }
    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State, qreal scale) override
    {
        QColor main = Theme::paper(), accent = Theme::ember();
        if (mode == QIcon::Disabled)
            main = accent = Icons::dim();
        else if (mode == QIcon::Active || mode == QIcon::Selected)
            main = accent = Theme::ember();
        return render(name_, size, &main, &accent, scale);  // size is logical, the result is size*scale pixels
    }
    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal dpr = painter->device()->devicePixelRatioF();
        const QPixmap pm = scaledPixmap(rect.size(), mode, state, dpr);
        painter->drawPixmap(rect.topLeft(), pm);
    }
    QString key() const override { return QStringLiteral("resora-svg"); }

private:
    QString name_;
};
}  // namespace

// Resources of a static library are only linked in when something refers to them.
static void initResources()
{
    Q_INIT_RESOURCE(resora);
}

namespace Icons {

QString resourcePath(const QString &name)
{
    static const bool once = (initResources(), true);
    Q_UNUSED(once);
    return name.contains(QLatin1Char('/')) ? QStringLiteral(":/%1.svg").arg(name)
                                           : QStringLiteral(":/icons/%1.svg").arg(name);
}

bool exists(const QString &name) { return QFile::exists(resourcePath(name)); }

QPixmap pixmap(const QString &name, const QSize &box, const QColor &main, const QColor &accent, qreal dpr)
{
    return render(name, box, &main, &accent, dpr);
}

QPixmap pixmap(const QString &name, int size, const QColor &main, const QColor &accent, qreal dpr)
{
    return render(name, QSize(size, size), &main, &accent, dpr);
}

QPixmap artwork(const QString &name, const QSize &box, qreal dpr)
{
    return render(name, box, nullptr, nullptr, dpr);
}

QIcon icon(const QString &name)
{
    return QIcon(new SvgIconEngine(name));
}

QColor dim() { return QColor(0x5e, 0x57, 0x4f); }

}  // namespace Icons
