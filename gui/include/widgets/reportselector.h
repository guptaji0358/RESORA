#pragma once
// One optional report destination: enable checkbox, path field, Browse (save dialog).
#include <QString>
#include <QWidget>

class LocationStore;
class LocationSuggester;
class QCheckBox;
class QLineEdit;
class QPushButton;

class ReportSelector : public QWidget
{
    Q_OBJECT
public:
    // extension without dot ("csv"); filterName like "CSV files".
    ReportSelector(const QString &title, const QString &extension, const QString &filterName,
                   QWidget *parent = nullptr);

    bool isWanted() const;
    void setWanted(bool on);
    void setPath(const QString &path);
    // Destination with the required extension appended when missing; empty if disabled.
    QString path() const;
    // Empty when valid or disabled. Missing parent folders are fine: the backend creates them.
    QString validationError() const;
    void setBusy(bool busy);
    // Enables Saved Locations recommendations on the path field (the store must outlive this widget).
    void setLocationStore(const LocationStore *store);
    QLineEdit *pathEdit() const { return edit_; }

private:
    void browse();
    void syncEnabled();

    QString extension_;
    QString filter_;
    QCheckBox *check_;
    QLineEdit *edit_;
    QPushButton *browse_;
    LocationSuggester *suggester_ = nullptr;
};
