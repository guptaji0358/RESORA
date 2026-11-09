#pragma once
// Export the current Analytics dashboard to a .resora file (Phase 6): mode choice (Bundled / Linked),
// destination, overwrite confirmation, progress and cancel. Work runs in an ExportJob (worker thread).
#include <functional>

#include "backend/resorajobs.h"
#include "widgets/chamferbutton.h"
#include "widgets/framelessdialog.h"

class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;

class ExportDialog : public FramelessDialog
{
    Q_OBJECT
public:
    ExportDialog(const dashboard::State &state, const QString &appVersion, QWidget *parent = nullptr);

    resoraui::Mode mode() const { return mode_; }
    void setMode(resoraui::Mode m);
    void setDestination(const QString &path);
    // Validates, confirms an overwrite, then starts the job. False if nothing was started.
    bool startExport();
    bool isRunning() const { return job_.isRunning(); }
    void cancelExport() { job_.cancel(); }
    const resoraui::ExportOutcome &outcome() const { return outcome_; }
    QString validationError() const { return check_.error; }
    // Replaces the overwrite prompt (default: ConfirmDialog); lets tests answer without UI.
    void setOverwriteConfirmer(std::function<bool(const QString &)> fn) { confirm_ = std::move(fn); }
    QPushButton *exportButton() const { return exportBtn_; }
    QLabel *statusLabel() const { return status_; }

protected:
    void reject() override;

private:
    void refresh();
    void browse();
    void onProgress(int permille, const QString &stage);
    void onFinished(const resoraui::ExportOutcome &o);
    void setBusy(bool busy);

    dashboard::State state_;
    QString appVersion_;
    resoraui::Estimate estimate_;
    resoraui::Mode mode_ = resoraui::Mode::Bundled;
    resoraui::DestinationCheck check_;
    resoraui::ExportJob job_;
    resoraui::ExportOutcome outcome_;
    std::function<bool(const QString &)> confirm_;
    bool closeWhenDone_ = false;

    QPushButton *bundled_ = nullptr, *linked_ = nullptr;
    QLabel *modeNote_ = nullptr, *error_ = nullptr, *status_ = nullptr;
    QLineEdit *dest_ = nullptr;
    QPushButton *browse_ = nullptr;
    QProgressBar *bar_ = nullptr;
    ChamferButton *exportBtn_ = nullptr, *cancelBtn_ = nullptr;
};
