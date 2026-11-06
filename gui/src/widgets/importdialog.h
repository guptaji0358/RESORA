#pragma once
// RESORA's import chooser (Ctrl+O): three tiles - folder, single image, multiple images. Each tile
// opens the matching native file dialog; the dialog closes once a usable choice exists.
#include "backend/importselection.h"
#include "framelessdialog.h"

class LocationStore;
class QLineEdit;

class ImportDialog : public FramelessDialog
{
    Q_OBJECT
public:
    // `store` (optional) powers Saved Locations recommendations on the folder path field.
    explicit ImportDialog(const QString &startDir, QWidget *parent = nullptr, const LocationStore *store = nullptr);

    ImportSelection selection() const { return selection_; }
    QLineEdit *pathEdit() const { return path_; }

    // What a tile does after its native dialog returns (also used directly by tests).
    void applyFolder(const QString &path);
    void applyFiles(const QStringList &paths);

private:
    void chooseFolder();
    void chooseSingle();
    void chooseMultiple();
    QWidget *makeTile(const QString &key, const QString &title, const QString &text, const QKeySequence &shortcut,
                      void (ImportDialog::*slot)());

    void importTypedFolder();

    QLineEdit *path_ = nullptr;
    QString startDir_;
    ImportSelection selection_;
};
