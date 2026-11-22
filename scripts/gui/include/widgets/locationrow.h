#pragma once
// One saved location in the Settings list: status bar, name, full path, availability note and the
// Rename / Change folder / Remove actions. Presentation only; it reports what the user asked for.
#include <QFrame>

#include "locations/savedlocation.h"

class ElidedLabel;
class QLabel;
class QLineEdit;
class ChamferButton;

class LocationRow : public QFrame
{
    Q_OBJECT
public:
    LocationRow(const SavedLocation &location, LocationStatus status, bool editable, QWidget *parent = nullptr);

    QString locationId() const { return location_.id; }
    LocationStatus status() const { return status_; }
    QString statusMessage() const;  // empty when the folder is available
    QString displayName() const { return location_.name; }
    QString fullPath() const;       // full, un-elided path (native separators)
    void beginRename();             // inline name editor; Enter commits, Esc cancels

signals:
    void renameRequested(const QString &id, const QString &newName);
    void changeFolderRequested(const QString &id);
    void removeRequested(const QString &id);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void endRename(bool commit);

    SavedLocation location_;
    LocationStatus status_;
    QLabel *name_;
    QLineEdit *nameEdit_;
    ElidedLabel *path_;
    QLabel *note_;
    ChamferButton *rename_;
    ChamferButton *change_;
    ChamferButton *remove_;
};
