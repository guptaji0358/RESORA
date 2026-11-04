#pragma once
// Presentation only: gathers input, starts a BackendProcess, shows progress and results.
#include <QElapsedTimer>
#include <QMainWindow>
#include <QPointer>

#include "backend/backendlocator.h"
#include "backend/scanrequest.h"

class AppHeader;
class BackendProcess;
class SourcePicker;
class QTemporaryFile;
class QCheckBox;
class ProgressPanel;
class QPushButton;
class QLabel;
class QStackedWidget;
class PhaseRail;
class ReportSelector;
class ResultsPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    bool isScanning() const;
    SourcePicker *sourcePicker() const { return source_; }

protected:
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &type, void *message, qintptr *result) override;
#endif
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void showPhase(int phase);
    void setNotice(const QString &text);
    void importSources();  // Ctrl+O
    void startScan();
    void cancelScan();
    bool validateInput(ScanRequest *request);
    bool confirmOverwrite(const QString &path);
    void setRunning(bool running);
    void onProgress(qint64 done, qint64 total, const QString &message);
    void onSucceeded(const QJsonValue &data);
    void onFailed(const QString &code, const QString &message, int exitCode, const QString &stderrTail);
    void onCancelled();
    void jobEnded();

    AppHeader *header_ = nullptr;
    PhaseRail *rail_ = nullptr;
    QStackedWidget *stack_ = nullptr;
    QLabel *notice_ = nullptr;
    QPushButton *again_ = nullptr;
    SourcePicker *source_ = nullptr;
    QCheckBox *recursive_ = nullptr;
    ReportSelector *csv_ = nullptr;
    ReportSelector *txt_ = nullptr;
    QPushButton *start_ = nullptr;
    QPushButton *cancel_ = nullptr;
    ProgressPanel *progress_ = nullptr;
    ResultsPanel *results_ = nullptr;

    QPointer<BackendProcess> job_;
    QTemporaryFile *paramsFile_ = nullptr;  // selected-image list for the running scan
    ScanRequest request_;
    QElapsedTimer sinceProgress_;
    bool closing_ = false;
};
