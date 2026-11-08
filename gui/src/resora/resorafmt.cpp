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

}  // namespace resora
