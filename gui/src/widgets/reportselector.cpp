#include "reportselector.h"

#include <QCheckBox>
#include "toggleswitch.h"
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include "chamferbutton.h"
#include <QStandardPaths>
#include "locationsuggester.h"


ReportSelector::ReportSelector(const QString &title, const QString &extension,
                               const QString &filterName, QWidget *parent)
    : QWidget(parent),
      extension_(extension),
      filter_(QStringLiteral("%1 (*.%2)").arg(filterName, extension)),
      check_(new ToggleSwitch(title, this)),
      edit_(new QLineEdit(this)),
      browse_(new ChamferButton(tr("Browse"), this))
{
    check_->setMinimumWidth(170);
    check_->setToolTip(tr("Also write a .%1 report when the scan finishes").arg(extension));
    edit_->setClearButtonEnabled(true);
    edit_->setAccessibleName(tr("%1 destination").arg(title));
    static_cast<ChamferButton *>(browse_)->setGlyph(QStringLiteral("folder-open"));
    browse_->setAccessibleName(tr("Browse for %1 destination").arg(title));
    browse_->setToolTip(tr("Choose where to save the .%1 report").arg(extension));

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(check_);
    row->addWidget(edit_, 1);
    row->addWidget(browse_);

    connect(check_, &QCheckBox::toggled, this, &ReportSelector::syncEnabled);
    connect(edit_, &QLineEdit::textChanged, this, [this](const QString &t) { edit_->setToolTip(t); });
    connect(browse_, &QPushButton::clicked, this, &ReportSelector::browse);
    syncEnabled();
}

bool ReportSelector::isWanted() const
{
    return check_->isChecked();
}

void ReportSelector::setWanted(bool on)
{
    check_->setChecked(on);
}

void ReportSelector::setPath(const QString &path)
{
    edit_->setText(path);
}

QString ReportSelector::path() const
{
    const QString raw = edit_->text().trimmed();
    if (!isWanted() || raw.isEmpty())
        return {};
    const QString suffix = QFileInfo(raw).suffix();
    if (suffix.compare(extension_, Qt::CaseInsensitive) == 0)
        return raw;
    return raw.endsWith(QLatin1Char('.')) ? raw + extension_ : raw + QLatin1Char('.') + extension_;
}

QString ReportSelector::validationError() const
{
    if (!isWanted())
        return {};
    if (edit_->text().trimmed().isEmpty())
        return tr("Choose a destination for the .%1 report, or untick it.").arg(extension_);
    if (QFileInfo(edit_->text().trimmed()).isDir())
        return tr("The .%1 destination is a folder; enter a file name.").arg(extension_);
    return {};
}

void ReportSelector::setLocationStore(const LocationStore *store)
{
    if (suggester_ || !store)
        return;
    suggester_ = new LocationSuggester(edit_, store, {LocationRecommender::Kind::ReportFile, extension_});
}

void ReportSelector::setBusy(bool busy)
{
    check_->setEnabled(!busy);
    edit_->setEnabled(!busy && isWanted());
    browse_->setEnabled(!busy && isWanted());
}

void ReportSelector::syncEnabled()
{
    edit_->setEnabled(isWanted());
    browse_->setEnabled(isWanted());
    edit_->setPlaceholderText(isWanted() ? tr("Destination .%1 file").arg(extension_)
                                         : tr("Tick the box to export a .%1 report").arg(extension_));
    edit_->setToolTip(edit_->text());
}

void ReportSelector::browse()
{
    QString start = path();
    if (start.isEmpty())
        start = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                    .filePath(QStringLiteral("image_resolution_report.%1").arg(extension_));
    // DontConfirmOverwrite: MainWindow asks once, right before the scan, for typed paths too.
    const QString file = QFileDialog::getSaveFileName(this, tr("Save .%1 report as").arg(extension_),
                                                      start, filter_, nullptr,
                                                      QFileDialog::DontConfirmOverwrite);
    if (!file.isEmpty())
        setPath(QDir::toNativeSeparators(file));
}
