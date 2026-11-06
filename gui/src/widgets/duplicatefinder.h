#pragma once
// Duplicate Finder: choose folders (or the imported collection), scan with the backend `duplicates`
// command, browse exact vs visually similar groups. Detection and comparison only: nothing here
// deletes, moves or renames a file. "Open", "Open folder" and "Compare" are read-only actions.
#include <QJsonValue>
#include <QPointer>

#include "backend/duplicatesmodel.h"
#include "framelessdialog.h"

class BackendProcess;
class ChamferButton;
class MetricCard;
class QLabel;
class QListWidget;
class QProgressBar;
class QSpinBox;
class QTemporaryFile;
class QVBoxLayout;
class QWidget;
class ToggleSwitch;

class DuplicateFinder : public FramelessDialog
{
    Q_OBJECT
public:
    // `collection`: the imported images (empty when nothing is imported). `cachePath`: fingerprint
    // cache file (empty = no cache; the default is a per-user file chosen by MainWindow).
    DuplicateFinder(const QStringList &collection, const QString &startFolder, const QString &cachePath,
                    QWidget *parent = nullptr);
    ~DuplicateFinder() override;

    void addFolder(const QString &path);
    void addExclusion(const QString &path);
    void setUseCollection(bool on);
    DupRequest request() const;
    QString validationError() const;
    void start();
    void cancel();
    bool isRunning() const { return job_ != nullptr; }

    // Show a parsed result (also what a successful run does).
    void showResult(const DupResult &result);
    int groupCount() const;             // groups in the current list
    void showKind(bool exact);          // switch the list between exact and similar
    void selectGroup(int row);
    int memberRowCount() const;         // rows currently shown for the selected group
    const DupResult &result() const { return result_; }

signals:
    // Two files the user wants to compare (the host opens its comparison workspace).
    void compareRequested(const QString &left, const QString &right);

private:
    void buildUi();
    void pickFolder();
    void pickExclusion();
    void refreshControls();
    void onProgress(qint64 done, qint64 total, const QString &message);
    void onSucceeded(const QJsonValue &data);
    void finish(const QString &text, bool error);
    void fillGroups();
    void fillMembers(int row);
    QWidget *memberRow(const DupGroup &group, int index);

    QStringList collection_;
    QString startFolder_, cachePath_;
    QListWidget *folders_ = nullptr, *excludes_ = nullptr, *groups_ = nullptr;
    ToggleSwitch *recursive_ = nullptr, *similar_ = nullptr, *useCollection_ = nullptr;
    QSpinBox *threshold_ = nullptr;
    ChamferButton *start_ = nullptr, *cancel_ = nullptr, *exactTab_ = nullptr, *similarTab_ = nullptr;
    QProgressBar *bar_ = nullptr;
    QLabel *status_ = nullptr, *kindNote_ = nullptr, *placeholder_ = nullptr;
    QWidget *resultsBox_ = nullptr, *membersHost_ = nullptr;
    QVBoxLayout *membersLayout_ = nullptr;
    MetricCard *cards_[8] = {};
    QPointer<BackendProcess> job_;
    QTemporaryFile *paramsFile_ = nullptr;
    DupResult result_;
    bool showExact_ = true;
};
