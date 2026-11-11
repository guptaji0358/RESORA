#include "resora/resorafmt.h"

namespace resora {

bool isValidResourceId(const QString &id)
{
    if (id.isEmpty() || id.size() > kMaxIdLength)
        return false;
    for (int i = 0; i < id.size(); ++i) {
        const ushort c = id.at(i).unicode();
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (!(alnum || (i > 0 && (c == '-' || c == '_'))))
            return false;
    }
    return true;
}

QString schemaVersionProblem(const QJsonValue &v, int supported, const QString &what)
{
    const double d = v.toDouble(-1);
    if (!v.isDouble() || d < 1 || d != double(int(d)))
        return QStringLiteral("%1 has no valid schema_version").arg(what);
    const int found = int(d);
    if (found == supported)
        return QString();
    if (found > supported)
        return QStringLiteral("%1 was written by a newer RESORA (schema_version %2; this build supports %3)")
            .arg(what).arg(found).arg(supported);
    return QStringLiteral("%1 uses schema_version %2, which this build cannot read (no migration exists)")
        .arg(what).arg(found);
}

}  // namespace resora
