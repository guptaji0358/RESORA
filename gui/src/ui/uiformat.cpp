#include "ui/uiformat.h"

#include <QLocale>

namespace UiFormat {

QString count(qint64 n)
{
    return QLocale(QLocale::English).toString(n);  // fixed grouping, independent of system locale
}

int percent(qint64 done, qint64 total)
{
    if (total <= 0)
        return -1;
    const qint64 d = qBound<qint64>(0, done, total);
    return static_cast<int>(d * 100 / total);
}

QString progressSummary(qint64 done, qint64 total)
{
    if (total <= 0)
        return {};
    return QObject::tr("%1 of %2 files (%3%)")
        .arg(count(qBound<qint64>(0, done, total)), count(total))
        .arg(percent(done, total));
}

}  // namespace UiFormat
