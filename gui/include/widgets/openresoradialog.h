#pragma once
// Open a .resora file (Phase 6): choose, validate (worker thread), resolve missing linked images, open.
// On accept(), session() holds the new dashboard; the caller swaps it in. The dialog never touches the
// currently displayed dashboard.
#include <memory>

#include "backend/resorajobs.h"
#include "widgets/framelessdialog.h"

class ChamferButton;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class RelinkPanel;

class OpenResoraDialog : public FramelessDialog
{
    Q_OBJECT
public:
    enum class State { Idle, Opening, NeedsRelink, Failed, Done };

    // `cleanup`: the application's shared cache-cleanup owner (see backend/cachecleanupmanager.h).
    explicit OpenResoraDialog(const QString &startDir = QString(), QWidget *parent = nullptr,
                              std::shared_ptr<dashboard::CacheCleanupManager> cleanup = {});

    State state() const { return state_; }
    void setFile(const QString &path);
    bool startOpen();            // first attempt with the chosen file
    bool retryWithRelinks();     // re-open with the chosen replacements (all images are revalidated)
    bool isRunning() const { return job_.isRunning(); }
    void cancelOpen() { job_.cancel(); }
    std::shared_ptr<dashboard::Session> session() const { return session_; }
    RelinkPanel *relinkPanel() const { return relink_; }
    QString message() const;
    QPushButton *openButton() const;

protected:
    void reject() override;

private:
    void begin(const resora::OpenOptions &opts);
    void onProgress(int permille, const QString &stage);
    void onFinished(const resoraui::ImportOutcome &o);
    void setState(State s);
    void browse();
    void chooseFolder();

    State state_ = State::Idle;
    QString startDir_, file_;
    resoraui::ImportJob job_;
    std::shared_ptr<dashboard::Session> session_;
    std::shared_ptr<dashboard::CacheCleanupManager> cleanup_;
    bool closeWhenDone_ = false;

    QLineEdit *path_ = nullptr;
    QLabel *status_ = nullptr;
    QProgressBar *bar_ = nullptr;
    RelinkPanel *relink_ = nullptr;
    QWidget *relinkTools_ = nullptr;
    ChamferButton *browse_ = nullptr, *open_ = nullptr, *close_ = nullptr, *folder_ = nullptr;
};
