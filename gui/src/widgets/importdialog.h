#pragma once
// RESORA's import chooser (Ctrl+O): three tiles - folder, single image, multiple images. Each tile
// opens the matching native file dialog; the dialog closes once a usable choice exists.
#include <QDialog>

#include "backend/importselection.h"

class ImportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ImportDialog(const QString &startDir, QWidget *parent = nullptr);

    ImportSelection selection() const { return selection_; }

    // What a tile does after its native dialog returns (also used directly by tests).
    void applyFolder(const QString &path);
    void applyFiles(const QStringList &paths);

private:
    void chooseFolder();
    void chooseSingle();
    void chooseMultiple();
    QWidget *makeTile(const QString &key, const QString &title, const QString &text, const QKeySequence &shortcut,
                      void (ImportDialog::*slot)());

    QString startDir_;
    ImportSelection selection_;
};
