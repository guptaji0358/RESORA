#pragma once
// Presentation only: gathers input, starts a BackendProcess, shows progress and results.
#include <QElapsedTimer>
#include <QMainWindow>
#include <QPointer>

#include "backend/backendlocator.h"
#include "backend/scanrequest.h"

class AppHeader;
class LocationStore;
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
class AnalyticsPanel;
class ComparisonWorkspace;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    // `locationsFile`: where Saved Locations are stored; empty = the per-user default (tests pass a temp file).
    explicit MainWindow(QWidget *parent = nullptr, const QString &locationsFile = QString());

    bool isScanning() const;
    SourcePicker *sourcePicker() const { return source_; }
    LocationStore *locationStore() const { return locations_; }
    ReportSelector *csvSelector() const { return csv_; }
    ReportSelector *txtSelector() const { return txt_; }
    AnalyticsPanel *analyticsPanel() const { return analytics_; }
    ComparisonWorkspace *comparison() const { return compare_; }
    bool isComparing() const;

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
    void openSettings();   // Ctrl+,
    void openProperties(); // Ctrl+I
    void openComparison(); // Ctrl+Shift+C
    void openDuplicates(); // Ctrl+Shift+D
    void closeComparison();
    void startScan();
    void startAnalytics();  // Refresh in the Analytics view: same import, no reports
    void launch(ScanRequest request);
    void updateAnalyticsControls();
    void cancelScan();
    bool validateInput(ScanRequest *request);
    bool confirmOverwrite(const QString &path);
    void setRunning(bool running);
    void onProgress(qint64 done, qint64 total, const QString &message);
    void onSucceeded(const QJsonValue &data);
    void onFailed(const QString &code, const QString &message, int exitCode, const QString &stderrTail);
    void onCancelled();
    void jobEnded();

    LocationStore *locations_ = nullptr;  // Saved Locations (owned by this window)
    AppHeader *header_ = nullptr;
    PhaseRail *rail_ = nullptr;
    QStackedWidget *stack_ = nullptr;
    QLabel *notice_ = nullptr;
    QPushButton *again_ = nullptr;
    SourcePicker *source_ = nullptr;
    QCheckBox *recursive_ = nullptr;
    ReportSelector *csv_ = nullptr;
    ReportSelector *txt_ = nullptr;
    QCheckBox *txtAnalytics_ = nullptr;
    QPushButton *start_ = nullptr;
    QPushButton *cancel_ = nullptr;
    ProgressPanel *progress_ = nullptr;
    ResultsPanel *results_ = nullptr;
    AnalyticsPanel *analytics_ = nullptr;
    ComparisonWorkspace *compare_ = nullptr;
    int compareReturn_ = 0;  // stack page to return to from the comparison workspace

    QPointer<BackendProcess> job_;
    QTemporaryFile *paramsFile_ = nullptr;  // selected-image list for the running scan
    ScanRequest request_;
    QElapsedTimer sinceProgress_;
    bool closing_ = false;
    bool analyticsJob_ = false;  // the running job is an analytics refresh, not a scan
};
