#pragma once
// Settings > Saved Locations: add form (name, folder, Browse), the list of locations with their
// availability, and load warnings. All changes go through LocationStore; this class only presents.
#include <QWidget>

#include "locations/locationstore.h"

class LocationRow;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

class SavedLocationsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SavedLocationsPanel(LocationStore *store, QWidget *parent = nullptr);

    // Actions behind the buttons (also used directly by tests). They show the outcome in the panel.
    LocationStore::Result addLocation(const QString &name, const QString &path);
    LocationStore::Result renameLocation(const QString &id, const QString &name);
    LocationStore::Result changeLocationPath(const QString &id, const QString &path);
    LocationStore::Result removeLocation(const QString &id);

    int rowCount() const { return rows_.size(); }
    LocationRow *rowAt(int i) const { return rows_.value(i); }
    QString message() const;        // last outcome / error text
    bool warningVisible() const;
    QLineEdit *nameEdit() const { return name_; }
    QLineEdit *pathEdit() const { return path_; }

protected:
    void showEvent(QShowEvent *) override;

private:
    void rebuild();
    void report(const LocationStore::Result &r, const QString &success);
    void browseForNew();
    void browseToChange(const QString &id);
    void confirmRemove(const QString &id);

    LocationStore *store_;
    QLineEdit *name_;
    QLineEdit *path_;
    QPushButton *browse_;
    QPushButton *add_;
    QLabel *message_;
    QLabel *warning_;
    QLabel *empty_;
    QVBoxLayout *list_;
    QList<LocationRow *> rows_;
};
