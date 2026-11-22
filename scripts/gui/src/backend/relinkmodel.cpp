#include "backend/relinkmodel.h"

#include <QDir>
#include <QObject>

namespace resoraui {

using Status = RelinkModel::Status;

void RelinkModel::clear() { rows_.clear(); }

void RelinkModel::update(const QList<resora::LinkStatus> &problems)
{
    QHash<QString, const resora::LinkStatus *> failing;
    for (const resora::LinkStatus &p : problems)
        failing.insert(p.imageId, &p);

    if (rows_.isEmpty()) {
        for (const resora::LinkStatus &p : problems) {
            Row r;
            r.imageId = p.imageId;
            r.name = p.name;
            r.recordedPath = p.recordedPath;
            r.message = p.message;
            r.roles = p.roles;
            r.status = p.state == resora::LinkState::IdentityMismatch ? Status::WrongImage : Status::Missing;
            rows_ << r;
        }
        return;
    }
    for (Row &r : rows_) {
        if (r.status == Status::Removed)
            continue;
        const auto it = failing.constFind(r.imageId);
        if (it != failing.constEnd()) {
            r.message = (*it)->message;
            if (r.status == Status::ReplacementChosen || r.status == Status::Resolved
                || (*it)->state == resora::LinkState::IdentityMismatch)
                r.status = Status::WrongImage;  // a rejected (or no longer valid) replacement is never kept
            r.replacement.clear();
        } else {
            r.status = Status::Resolved;  // verified by the engine in this round
            r.message.clear();
        }
    }
}

RelinkModel::Row *RelinkModel::mut(const QString &imageId)
{
    for (Row &r : rows_)
        if (r.imageId == imageId)
            return &r;
    return nullptr;
}

const RelinkModel::Row *RelinkModel::find(const QString &imageId) const
{
    for (const Row &r : rows_)
        if (r.imageId == imageId)
            return &r;
    return nullptr;
}

bool RelinkModel::assign(const QString &imageId, const QString &path)
{
    Row *r = mut(imageId);
    if (!r || r->status == Status::Removed || path.isEmpty())
        return false;
    r->replacement = QDir::fromNativeSeparators(path);
    r->status = Status::ReplacementChosen;
    return true;
}

bool RelinkModel::clearReplacement(const QString &imageId)
{
    Row *r = mut(imageId);
    if (!r || r->replacement.isEmpty())
        return false;
    // The association returns to the unresolved state; it is neither removed nor left accepted.
    r->replacement.clear();
    r->status = Status::Missing;
    return true;
}

bool RelinkModel::remove(const QString &imageId)
{
    Row *r = mut(imageId);
    if (!r)
        return false;
    r->replacement.clear();
    r->status = Status::Removed;
    return true;
}

QList<resora::LinkStatus> RelinkModel::unresolved() const
{
    QList<resora::LinkStatus> out;
    for (const Row &r : rows_)
        if (r.status == Status::Missing || r.status == Status::WrongImage) {
            resora::LinkStatus s;
            s.imageId = r.imageId;
            s.name = r.name;
            s.recordedPath = r.recordedPath;
            out << s;
        }
    return out;
}

QHash<QString, QString> RelinkModel::relinks() const
{
    QHash<QString, QString> h;
    for (const Row &r : rows_)
        if ((r.status == Status::ReplacementChosen || r.status == Status::Resolved) && !r.replacement.isEmpty())
            h.insert(r.imageId, r.replacement);
    return h;
}

QSet<QString> RelinkModel::omitted() const
{
    QSet<QString> s;
    for (const Row &r : rows_)
        if (r.status == Status::Removed)
            s.insert(r.imageId);
    return s;
}

int RelinkModel::count(Status s) const
{
    int n = 0;
    for (const Row &r : rows_)
        n += r.status == s ? 1 : 0;
    return n;
}

bool RelinkModel::complete() const
{
    return !rows_.isEmpty() && count(Status::Missing) == 0 && count(Status::WrongImage) == 0;
}

QString RelinkModel::statusText(Status s)
{
    switch (s) {
    case Status::Missing: return QObject::tr("Missing");
    case Status::WrongImage: return QObject::tr("Wrong Image");
    case Status::ReplacementChosen: return QObject::tr("Replacement chosen");
    case Status::Resolved: return QObject::tr("Resolved");
    case Status::Removed: return QObject::tr("Removed from Dashboard");
    }
    return {};
}

}  // namespace resoraui
