#pragma once
// Scan status: state badge, message, progress bar, "N of M files" and optional diagnostics.
// Never invents progress: the bar is indeterminate unless the backend supplied a total.
#include <QWidget>

class ElidedLabel;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class Viewfinder;

class ProgressPanel : public QWidget
{
    Q_OBJECT
public:
    enum class State { Idle, Starting, Running, Cancelling, Succeeded, Failed, Cancelled };

    explicit ProgressPanel(QWidget *parent = nullptr);

    State state() const { return state_; }
    bool isIndeterminate() const;

    void reset();
    void setStarting();
    void setProgress(qint64 done, qint64 total, const QString &message);
    void setCancelling();
    void setSucceeded(const QString &text);
    void setFailed(const QString &text, const QString &diagnostics = QString());
    void setCancelled(const QString &text);
signals:
    void stateChanged(const QString &label, const QString &name);

public:
    // Message without a state change (validation problems before a scan starts).
    void setNotice(const QString &text, bool isError);

private:
    void setState(State s, const QString &text);
    void finishBar(const QColor &color);
    void setBarColor(const QColor &color);
    void syncRing();

    State state_ = State::Idle;
    QLabel *badge_;
    QLabel *message_;
    ElidedLabel *item_;
    QLabel *counts_;
    QProgressBar *bar_;
    Viewfinder *finder_;
    QPlainTextEdit *details_;
};
