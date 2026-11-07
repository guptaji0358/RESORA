#pragma once
// Source page block: shows what was imported (kind, name, location, count, skipped files) with a
// Change button. Opens the ImportDialog (Ctrl+O); also accepts dropped folders / image files.
#include <QWidget>

#include "backend/importselection.h"

class ElidedLabel;
class LocationStore;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QPushButton;

class SourcePicker : public QWidget
{
    Q_OBJECT
public:
    explicit SourcePicker(QWidget *parent = nullptr);

    ImportSelection selection() const { return selection_; }
    void setSelection(const ImportSelection &selection);
    QString validationError() const;  // empty when something scannable is imported
    void setBusy(bool busy);
    void openImportDialog();          // modal; no-op while busy
    void setLocationStore(const LocationStore *store) { locations_ = store; }  // Saved Locations in the dialog

signals:
    void selectionChanged();

protected:
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    void refresh();

    ImportSelection selection_;
    const LocationStore *locations_ = nullptr;
    bool busy_ = false;
    QLabel *kind_;
    QLabel *title_;
    ElidedLabel *location_;
    QLabel *detail_;
    QLabel *notes_;
    QPushButton *change_;
    QLabel *kindIcon_;
};
