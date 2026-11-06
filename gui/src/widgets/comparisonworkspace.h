#pragma once
// Image Comparison workspace: two image slots (A | B), a synchronised inspection viewer, view
// controls and the technical comparison. No folder scan is needed: previews are decoded by Qt
// (PreviewLoader, worker threads, capped size) and metadata comes from the existing backend
// `properties` command, one small job per image, cached by path. Originals are only read.
#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QWidget>

#include "backend/comparison.h"
#include "backend/importselection.h"
#include "backend/previewloader.h"

class BackendProcess;
class ChamferButton;
class ComparisonTable;
class ComparisonViewer;
class ElidedLabel;
class QLabel;
class QTemporaryFile;
class ToggleSwitch;

class ComparisonWorkspace : public QWidget
{
    Q_OBJECT
public:
    enum Side { Left = 0, Right = 1 };
    explicit ComparisonWorkspace(QWidget *parent = nullptr);
    ~ComparisonWorkspace() override;

    // Validates (exists, is a file, known image extension) and loads. Returns an error text and
    // leaves the slot unchanged when rejected; corrupted files are accepted here and reported by
    // the pane/table once decoding fails.
    QString setImage(Side side, const QString &path);
    void clearImage(Side side);
    void swapImages();
    QString path(Side side) const { return paths_[side]; }
    bool hasBoth() const { return !paths_[0].isEmpty() && !paths_[1].isEmpty(); }

    // The imported collection ("From collection…"); a folder import is listed lazily (names only).
    void setCollection(const ImportSelection &selection, bool recursive);
    QStringList collectionFiles() const;

    ComparisonViewer *viewer() const { return viewer_; }
    ComparisonTable *table() const { return table_; }
    const Comparison &comparison() const { return comparison_; }
    bool isBusy() const { return !pendingPreviews_.isEmpty() || !pendingProps_.isEmpty(); }
    QString notice() const;

    void replaceFromDisk(Side side);        // file dialog
    void replaceFromCollection(Side side);  // collection picker

signals:
    void backRequested();
    void changed();  // selection, previews or comparison updated

private:
    void buildUi();
    void ensureLoaded(const QString &path);
    void startProperties(const QString &path);
    void onPropsDone(const QString &path, const QJsonObject &props);
    void trimCaches();
    void refresh();
    void refreshHeader(Side side);
    void setNotice(const QString &text);

    QString paths_[2];
    QString shown_[2];  // path currently displayed in each pane (keeps zoom when nothing changed)
    QHash<QString, PreviewResult> previews_;
    QHash<QString, QJsonObject> props_;
    QSet<QString> pendingPreviews_, pendingProps_;
    QHash<QString, QPointer<BackendProcess>> jobs_;
    QHash<QString, QTemporaryFile *> paramFiles_;
    ImportSelection collection_;
    bool collectionRecursive_ = true;
    Comparison comparison_;

    ComparisonViewer *viewer_ = nullptr;
    ComparisonTable *table_ = nullptr;
    ElidedLabel *name_[2] = {nullptr, nullptr};
    QLabel *facts_[2] = {nullptr, nullptr};
    ChamferButton *replace_[2] = {nullptr, nullptr};
    ChamferButton *fromCollection_[2] = {nullptr, nullptr};
    ChamferButton *clear_[2] = {nullptr, nullptr};
    ChamferButton *swap_ = nullptr;
    QLabel *notice_ = nullptr;
};
