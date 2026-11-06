#include "mainwindow.h"

#include "widgets/analyticspanel.h"
#include "widgets/comparisonworkspace.h"
#include "widgets/duplicatefinder.h"
#include <QStandardPaths>
#include "widgets/propertiesdialog.h"

#include <QCheckBox>
#include <QPainter>
#include <QShortcut>
#include <QStackedWidget>
#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QFrame>
#include <QLabel>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryFile>
#include <QVBoxLayout>

#include "backend/backendprocess.h"
#include "locations/locationstore.h"
#include "widgets/settingsdialog.h"
#include "widgets/appheader.h"
#include "widgets/chamferbutton.h"
#include "widgets/phaserail.h"
#include "widgets/toggleswitch.h"
#include "widgets/sourcepicker.h"
#include "ui/brand.h"
#include "ui/theme.h"
#include "widgets/aboutdialog.h"
#include "widgets/progresspanel.h"
#include "widgets/reportselector.h"
#include "widgets/resultspanel.h"
#include "widgets/sectioncard.h"

namespace {
constexpr qint64 kProgressIntervalMs = 100;  // UI refresh cap; backend already throttles
}

MainWindow::MainWindow(QWidget *parent, const QString &locationsFile) : QMainWindow(parent)
{
    locations_ = new LocationStore(locationsFile.isEmpty() ? LocationStore::defaultFilePath() : locationsFile, this);
    setWindowTitle(Brand::name() + QStringLiteral(" - ") + Brand::description());
    setWindowIcon(Brand::icon());
    setWindowFlag(Qt::FramelessWindowHint);
    setMinimumSize(960, 640);
    resize(1180, 800);
    setStyleSheet(Theme::styleSheet());
    buildUi();
    setRunning(false);
    updateAnalyticsControls();
}

bool MainWindow::isScanning() const
{
    return job_ && job_->isRunning();
}

namespace {
// Stage backdrop: a faint registration-dot grid, like a contact sheet / light table.
class GridStage : public QWidget
{
public:
    explicit GridStage(QWidget *p = nullptr) : QWidget(p) { setObjectName(QStringLiteral("stage")); }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), Theme::ink());
        p.setPen(QColor(255, 255, 255, 16));
        for (int y = 12; y < height(); y += 28)
            for (int x = 12; x < width(); x += 28)
                p.drawPoint(x, y);
    }
};

QLabel *makeLabel(const QString &text, const char *role)
{
    auto *l = new QLabel(text);
    l->setProperty("role", QString::fromLatin1(role));
    l->setWordWrap(true);
    return l;
}

// A phase page: eyebrow + display heading + body, content kept to a readable width.
QWidget *makePage(const QString &eyebrow, const QString &title, const QString &lede, QVBoxLayout **body, int maxWidth = 780)
{
    auto *page = new GridStage;
    auto *outer = new QHBoxLayout(page);
    outer->setContentsMargins(48, 36, 48, 36);
    auto *col = new QVBoxLayout;
    col->setSpacing(14);
    col->addWidget(makeLabel(eyebrow, "eyebrow"));
    col->addWidget(makeLabel(title, "display"));
    if (!lede.isEmpty())
        col->addWidget(makeLabel(lede, "lede"));
    col->addSpacing(10);
    *body = new QVBoxLayout;
    (*body)->setSpacing(18);
    col->addLayout(*body);
    col->addStretch(1);
    auto *holder = new QWidget;
    holder->setMaximumWidth(maxWidth);
    holder->setLayout(col);
    outer->addWidget(holder, 1);
    outer->addStretch(0);
    return page;
}
}  // namespace

void MainWindow::buildUi()
{
    header_ = new AppHeader;
    rail_ = new PhaseRail;
    stack_ = new QStackedWidget;

    // --- 01 Source ---
    QVBoxLayout *src = nullptr;
    stack_->addWidget(makePage(tr("01 — SOURCE"), tr("Import your images."),
                               Brand::tagline() + tr(" Only image headers are read; your originals are never modified."), &src));
    source_ = new SourcePicker;
    recursive_ = new ToggleSwitch(tr("Include subfolders"));
    recursive_->setChecked(true);
    recursive_->setToolTip(tr("Also analyze images inside every folder below the selected one"));
    source_->setLocationStore(locations_);
    src->addWidget(source_);
    src->addWidget(recursive_);

    csv_ = new ReportSelector(tr("CSV report"), QStringLiteral("csv"), tr("CSV files"));
    txt_ = new ReportSelector(tr("TXT report"), QStringLiteral("txt"), tr("Text files"));
    csv_->setLocationStore(locations_);
    txt_->setLocationStore(locations_);
    auto *reportCard = new SectionCard(tr("Export reports"), tr("Optional. Each format is independent."));
    reportCard->body()->addWidget(csv_);
    reportCard->body()->addWidget(txt_);
    txtAnalytics_ = new ToggleSwitch(tr("Add analytics summary to the TXT report"));
    txtAnalytics_->setToolTip(tr("Appends the Analytics statistics to the TXT report. Off by default; the CSV is never changed."));
    reportCard->body()->addWidget(txtAnalytics_);
    src->addWidget(reportCard);

    notice_ = makeLabel(QString(), "error");
    notice_->setAccessibleName(tr("Problem"));
    notice_->hide();
    src->addWidget(notice_);

    start_ = new ChamferButton(tr("Start Scan"));
    static_cast<ChamferButton *>(start_)->setGlyph(QStringLiteral("scan"));
    start_->setToolTip(tr("Analyze the imported folder or images"));
    Theme::setProperty(start_, "variant", QStringLiteral("primary"));
    start_->setMinimumHeight(46);
    auto *go = new QHBoxLayout;
    go->addWidget(start_);
    auto *compareSrc = new ChamferButton(tr("Compare Images"));
    compareSrc->setGlyph(QStringLiteral("swap"));
    compareSrc->setMinimumHeight(46);
    compareSrc->setToolTip(tr("Compare two images side by side; no scan needed (Ctrl+Shift+C)"));
    connect(compareSrc, &QPushButton::clicked, this, &MainWindow::openComparison);
    go->addWidget(compareSrc);
    auto *dupSrc = new ChamferButton(tr("Find Duplicates"));
    dupSrc->setGlyph(QStringLiteral("images"));
    dupSrc->setMinimumHeight(46);
    dupSrc->setToolTip(tr("Find identical and visually similar images (Ctrl+Shift+D); never deletes anything"));
    connect(dupSrc, &QPushButton::clicked, this, &MainWindow::openDuplicates);
    go->addWidget(dupSrc);
    go->addStretch(1);
    src->addLayout(go);

    // --- 02 Scan ---
    QVBoxLayout *scan = nullptr;
    stack_->addWidget(makePage(tr("02 — SCAN"), tr("Exposing."), QString(), &scan));
    progress_ = new ProgressPanel;
    scan->addWidget(progress_);
    cancel_ = new ChamferButton(tr("Cancel Scan"));
    cancel_->setToolTip(tr("Stop the running scan; reports from a cancelled scan are not valid"));
    static_cast<ChamferButton *>(cancel_)->setGlyph(QStringLiteral("stop"));
    Theme::setProperty(cancel_, "variant", QStringLiteral("danger"));
    auto *scanRow = new QHBoxLayout;
    scanRow->addWidget(cancel_);
    scanRow->addStretch(1);
    scan->addLayout(scanRow);

    // --- 03 Readout ---
    QVBoxLayout *out = nullptr;
    stack_->addWidget(makePage(tr("03 — READOUT"), tr("Resolution readout."), QString(), &out));
    results_ = new ResultsPanel;
    out->addWidget(results_);
    again_ = new ChamferButton(tr("New Scan"));
    static_cast<ChamferButton *>(again_)->setGlyph(QStringLiteral("refresh"));
    Theme::setProperty(again_, "variant", QStringLiteral("primary"));
    auto *props = new ChamferButton(tr("Image Properties"));
    props->setGlyph(QStringLiteral("image"));
    props->setToolTip(tr("Detailed properties of an image (Ctrl+I)"));
    connect(props, &QPushButton::clicked, this, &MainWindow::openProperties);
    auto *analyticsBtn = new ChamferButton(tr("Analytics"));
    analyticsBtn->setGlyph(QStringLiteral("scan"));
    analyticsBtn->setToolTip(tr("Statistics for this collection (Ctrl+4)"));
    connect(analyticsBtn, &QPushButton::clicked, this, [this] { showPhase(PhaseRail::Analytics); });
    auto *outRow = new QHBoxLayout;
    outRow->setSpacing(12);
    outRow->addWidget(again_);
    outRow->addWidget(props);
    outRow->addWidget(analyticsBtn);
    auto *compareOut = new ChamferButton(tr("Compare Images"));
    compareOut->setGlyph(QStringLiteral("swap"));
    compareOut->setToolTip(tr("Compare two images side by side (Ctrl+Shift+C)"));
    connect(compareOut, &QPushButton::clicked, this, &MainWindow::openComparison);
    outRow->addWidget(compareOut);
    outRow->addStretch(1);
    out->addLayout(outRow);

    // --- 04 Analytics ---
    QVBoxLayout *ana = nullptr;
    stack_->addWidget(makePage(tr("04 — ANALYTICS"), tr("Collection analytics."), QString(), &ana, 1020));
    analytics_ = new AnalyticsPanel;
    ana->addWidget(analytics_);

    // --- 05 Compare (workspace, reached by button / Ctrl+Shift+C; not a scan phase) ---
    compare_ = new ComparisonWorkspace;
    stack_->addWidget(compare_);
    connect(compare_, &ComparisonWorkspace::backRequested, this, &MainWindow::closeComparison);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(stack_);

    auto *shell = new QWidget;
    auto *shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);
    shellLayout->addWidget(header_);
    auto *body = new QHBoxLayout;
    body->setSpacing(0);
    body->addWidget(rail_);
    body->addWidget(scroll, 1);
    shellLayout->addLayout(body, 1);
    setCentralWidget(shell);
    // Start with focus on the neutral shell, not on an action button: a stray Enter does nothing.
    shell->setFocusPolicy(Qt::ClickFocus);
    shell->setFocus(Qt::OtherFocusReason);

    connect(start_, &QPushButton::clicked, this, &MainWindow::startScan);
    connect(analytics_, &AnalyticsPanel::refreshRequested, this, &MainWindow::startAnalytics);
    connect(analytics_, &AnalyticsPanel::propertiesRequested, this, [this](const QString &path) {
        PropertiesDialog(QStringList{path}, this).exec();
    });
    connect(cancel_, &QPushButton::clicked, this, &MainWindow::cancelScan);
    connect(again_, &QPushButton::clicked, this, [this] { showPhase(PhaseRail::Source); });
    connect(progress_, &ProgressPanel::stateChanged, header_, &AppHeader::setState);
    connect(rail_, &PhaseRail::phaseRequested, this, [this](int p) { stack_->setCurrentIndex(p); });
    auto *open = new QShortcut(QKeySequence::Open, this);  // Ctrl+O
    connect(open, &QShortcut::activated, this, &MainWindow::importSources);
    connect(source_, &SourcePicker::selectionChanged, this, [this] {
        analytics_->setStale(true);
        updateAnalyticsControls();
        setNotice(QString());
        recursive_->setEnabled(!isScanning() && source_->selection().kind == ImportSelection::Folder);
        recursive_->setToolTip(source_->selection().kind == ImportSelection::Folder
                                   ? tr("Also analyze images inside every folder below the selected one")
                                   : tr("Applies to folder imports only"));
    });
    connect(header_, &AppHeader::settingsRequested, this, &MainWindow::openSettings);
    auto *settings = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma), this);
    connect(settings, &QShortcut::activated, this, &MainWindow::openSettings);
    auto *propsKey = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_I), this);
    connect(propsKey, &QShortcut::activated, this, &MainWindow::openProperties);
    auto *cmpKey = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), this);
    connect(cmpKey, &QShortcut::activated, this, &MainWindow::openComparison);
    auto *dupKey = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), this);
    connect(dupKey, &QShortcut::activated, this, &MainWindow::openDuplicates);
    auto *about = new QShortcut(QKeySequence(Qt::Key_F1), this);
    connect(about, &QShortcut::activated, this, [this] { AboutDialog(this).exec(); });
    for (int i = 0; i < 4; ++i) {
        auto *sc = new QShortcut(QKeySequence(Qt::CTRL | (Qt::Key_1 + i)), this);
        connect(sc, &QShortcut::activated, this, [this, i] {
            if (rail_->isPhaseAvailable(PhaseRail::Phase(i)))
                showPhase(i);
        });
    }
}

void MainWindow::showPhase(int phase)
{
    stack_->setCurrentIndex(phase);
    rail_->setCurrent(PhaseRail::Phase(phase));
}

void MainWindow::importSources()
{
    if (isScanning())
        return;
    if (stack_->currentIndex() != PhaseRail::Source)
        showPhase(PhaseRail::Source);
    source_->openImportDialog();
}

void MainWindow::openSettings()
{
    SettingsDialog(locations_, this).exec();
}

void MainWindow::openProperties()
{
    // Preselect the imported images (single/multiple); a folder import asks the user to choose.
    QStringList files = source_->selection().files;
    if (source_->selection().kind == ImportSelection::Folder)
        files.clear();
    PropertiesDialog(files.mid(0, 500), this).exec();
}

void MainWindow::openDuplicates()  // Ctrl+Shift+D
{
    const ImportSelection sel = source_->selection();
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                          + QStringLiteral("/duplicates-cache.sqlite");
    const QStringList images = sel.kind == ImportSelection::Folder ? QStringList() : sel.files;
    DuplicateFinder dlg(images, sel.kind == ImportSelection::Folder ? sel.folder : QString(), cache, this);
    if (sel.kind == ImportSelection::Folder)
        dlg.addFolder(sel.folder);
    else
        dlg.setUseCollection(true);
    QString left, right;
    connect(&dlg, &DuplicateFinder::compareRequested, &dlg, [&](const QString &a, const QString &b) {
        left = a;
        right = b;
        dlg.accept();
    });
    dlg.exec();
    if (!left.isEmpty() && !right.isEmpty() && !isComparing()) {
        openComparison();
        compare_->setImage(ComparisonWorkspace::Left, left);
        compare_->setImage(ComparisonWorkspace::Right, right);
    }
}

bool MainWindow::isComparing() const
{
    return stack_->currentWidget() == compare_;
}

void MainWindow::openComparison()
{
    if (isComparing())
        return;
    compareReturn_ = stack_->currentIndex();
    compare_->setCollection(source_->selection(), recursive_->isChecked());
    // Empty slots start from the imported images (single / multiple), if any; no scan involved.
    const QStringList files = source_->selection().kind == ImportSelection::Folder ? QStringList() : source_->selection().files;
    for (int s = 0; s < 2 && s < files.size(); ++s)
        if (compare_->path(ComparisonWorkspace::Side(s)).isEmpty())
            compare_->setImage(ComparisonWorkspace::Side(s), files.at(s));
    stack_->setCurrentWidget(compare_);
}

void MainWindow::closeComparison()
{
    stack_->setCurrentIndex(compareReturn_);
}

void MainWindow::setNotice(const QString &text)
{
    notice_->setText(text);
    notice_->setVisible(!text.isEmpty());
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray &type, void *message, qintptr *result)
{
    // Frameless window: keep native edge resizing by answering the hit test near the borders.
    if (type == "windows_generic_MSG" && !isMaximized()) {
        const MSG *msg = static_cast<const MSG *>(message);
        if (msg->message == WM_NCHITTEST) {
            const int b = 7;
            RECT r;
            GetWindowRect(msg->hwnd, &r);
            const int x = GET_X_LPARAM(msg->lParam), y = GET_Y_LPARAM(msg->lParam);
            const bool l = x < r.left + b, rt = x >= r.right - b, t = y < r.top + b, bt = y >= r.bottom - b;
            if (t && l) { *result = HTTOPLEFT; return true; }
            if (t && rt) { *result = HTTOPRIGHT; return true; }
            if (bt && l) { *result = HTBOTTOMLEFT; return true; }
            if (bt && rt) { *result = HTBOTTOMRIGHT; return true; }
            if (l) { *result = HTLEFT; return true; }
            if (rt) { *result = HTRIGHT; return true; }
            if (t) { *result = HTTOP; return true; }
            if (bt) { *result = HTBOTTOM; return true; }
        }
    }
    return QMainWindow::nativeEvent(type, message, result);
}
#endif

void MainWindow::setRunning(bool running)
{
    start_->setEnabled(!running);
    cancel_->setEnabled(running);
    source_->setBusy(running);
    recursive_->setEnabled(!running && source_->selection().kind == ImportSelection::Folder);
    csv_->setBusy(running);
    txt_->setBusy(running);
    txtAnalytics_->setEnabled(!running);
    if (analytics_)
        updateAnalyticsControls();
}

void MainWindow::updateAnalyticsControls()
{
    analytics_->setRefreshEnabled(!isScanning() && source_->validationError().isEmpty());
}

bool MainWindow::confirmOverwrite(const QString &path)
{
    if (!QFileInfo::exists(path))
        return true;
    return QMessageBox::question(this, tr("Overwrite report?"),
                                 tr("%1 already exists. Replace it?").arg(QDir::toNativeSeparators(path)),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
           == QMessageBox::Yes;
}

bool MainWindow::validateInput(ScanRequest *request)
{
    QString error = source_->validationError();
    if (error.isEmpty())
        error = csv_->validationError();
    if (error.isEmpty())
        error = txt_->validationError();
    if (error.isEmpty() && !csv_->path().isEmpty()
        && QFileInfo(csv_->path()).absoluteFilePath().compare(
               QFileInfo(txt_->path()).absoluteFilePath(), Qt::CaseInsensitive) == 0)
        error = tr("The CSV and TXT reports need different files.");
    if (!error.isEmpty()) {
        setNotice(error);
        return false;
    }
    if ((!csv_->path().isEmpty() && !confirmOverwrite(csv_->path()))
        || (!txt_->path().isEmpty() && !confirmOverwrite(txt_->path()))) {
        setNotice(tr("Scan not started: existing report was not replaced."));
        return false;
    }
    const ImportSelection selection = source_->selection();
    if (selection.kind == ImportSelection::Folder) {
        request->folder = selection.folder;
        request->recursive = recursive_->isChecked();
    } else {
        request->files = selection.files;
        request->recursive = false;
    }
    request->csvPath = csv_->path().isEmpty() ? QString() : QFileInfo(csv_->path()).absoluteFilePath();
    request->txtPath = txt_->path().isEmpty() ? QString() : QFileInfo(txt_->path()).absoluteFilePath();
    request->txtAnalytics = txtAnalytics_->isChecked();
    return true;
}

void MainWindow::startScan()
{
    if (isScanning())
        return;
    ScanRequest request;
    if (!validateInput(&request))
        return;
    analyticsJob_ = false;
    launch(request);
}

void MainWindow::startAnalytics()
{
    if (isScanning())
        return;
    const QString error = source_->validationError();
    if (!error.isEmpty()) {
        setNotice(error);
        return;
    }
    ScanRequest request;
    const ImportSelection selection = source_->selection();
    if (selection.kind == ImportSelection::Folder) {
        request.folder = selection.folder;
        request.recursive = recursive_->isChecked();
    } else {
        request.files = selection.files;
        request.recursive = false;
    }
    request.command = QStringLiteral("analytics");  // no reports are written
    analyticsJob_ = true;
    launch(request);
}

void MainWindow::launch(ScanRequest request)
{
    const BackendLocation location =
        locateBackend(QProcessEnvironment::systemEnvironment(), QCoreApplication::applicationDirPath());
    if (!location.ok()) {
        setNotice(location.error);
        return;
    }

    if (request.usesParamsFile()) {  // the file list travels in the params file, not on the command line
        auto *file = new QTemporaryFile(QDir::temp().filePath(QStringLiteral("resora-scan-XXXXXX.json")), this);
        if (!file->open() || file->write(request.paramsJson()) < 0 || !file->flush()) {
            setNotice(tr("Could not write the temporary selection file: %1").arg(file->errorString()));
            delete file;
            return;
        }
        file->close();  // keep the file on disk (autoRemove) for the backend to read
        request.paramsFile = file->fileName();
        delete paramsFile_;
        paramsFile_ = file;
    }

    request_ = request;
    setNotice(QString());
    if (!analyticsJob_) {
        results_->clear();
        rail_->setAvailable(PhaseRail::Readout, false);
    }
    rail_->setAvailable(PhaseRail::Scan, true);
    showPhase(PhaseRail::Scan);
    progress_->setStarting();  // indeterminate until the backend reports a total
    sinceProgress_.invalidate();

    job_ = new BackendProcess(this);
    connect(job_, &BackendProcess::progress, this, &MainWindow::onProgress);
    connect(job_, &BackendProcess::succeeded, this, &MainWindow::onSucceeded);
    connect(job_, &BackendProcess::failed, this, &MainWindow::onFailed);
    connect(job_, &BackendProcess::cancelled, this, &MainWindow::onCancelled);
    setRunning(true);
    job_->start(location, request_.commandArguments());
}

void MainWindow::cancelScan()
{
    if (!isScanning())
        return;
    cancel_->setEnabled(false);
    progress_->setCancelling();
    job_->cancel();
}

void MainWindow::onProgress(qint64 done, qint64 total, const QString &message)
{
    const bool last = total > 0 && done >= total;
    if (sinceProgress_.isValid() && sinceProgress_.elapsed() < kProgressIntervalMs && !last)
        return;
    sinceProgress_.restart();
    progress_->setProgress(done, total, message);
}

void MainWindow::jobEnded()
{
    delete paramsFile_;  // removes the temporary params file
    paramsFile_ = nullptr;
    setRunning(false);
    if (job_)
        job_->deleteLater();
    if (closing_)
        close();
}

void MainWindow::onSucceeded(const QJsonValue &data)
{
    analytics_->showAnalytics(data.toObject());
    updateAnalyticsControls();
    if (analyticsJob_) {  // refresh: the scan readout and reports stay as they were
        progress_->setSucceeded(tr("Analytics refreshed."));
        showPhase(PhaseRail::Analytics);
        jobEnded();
        return;
    }
    results_->showResult(data.toObject(), !request_.csvPath.isEmpty(), !request_.txtPath.isEmpty());
    const int total = data.toObject().value(QStringLiteral("summary")).toObject().value(QStringLiteral("total")).toInt();
    progress_->setSucceeded(tr("Scan complete: %1 file(s) processed.").arg(total));
    rail_->setAvailable(PhaseRail::Readout, true);
    showPhase(PhaseRail::Readout);
    jobEnded();
}

void MainWindow::onFailed(const QString &code, const QString &message, int exitCode, const QString &stderrTail)
{
    QString text = message.isEmpty() ? code : message;
    if (code == QLatin1String("report_failed"))
        text = tr("The scan ran, but a report could not be written (no reports were confirmed): %1").arg(message);
    else
        text = (analyticsJob_ ? tr("Analytics failed [%1]: %2") : tr("Scan failed [%1]: %2")).arg(code, text);
    if (exitCode > 0)
        text += tr(" (exit code %1)").arg(exitCode);
    progress_->setFailed(text, stderrTail);
    jobEnded();
}

void MainWindow::onCancelled()
{
    progress_->setCancelled(analyticsJob_ ? tr("Analytics refresh cancelled.")
                                          : tr("Scan cancelled. Reports from a cancelled scan are not valid."));
    jobEnded();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!isScanning()) {
        event->accept();
        return;
    }
    closing_ = true;  // finish via the normal cancel -> grace -> kill path, then close
    cancelScan();
    event->ignore();
}
