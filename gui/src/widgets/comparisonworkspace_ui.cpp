// UI half of ComparisonWorkspace: layout, per-side headers, refresh and the pick dialogs.
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollArea>
#include <QShortcut>
#include <QSplitter>
#include <QVBoxLayout>

#include "backend/propertiesmodel.h"
#include "widgets/chamferbutton.h"
#include "widgets/comparisontable.h"
#include "widgets/comparisonviewer.h"
#include "widgets/comparisonworkspace.h"
#include "widgets/elidedlabel.h"
#include "widgets/framelessdialog.h"
#include "widgets/toggleswitch.h"
#include "ui/theme.h"

namespace {
ChamferButton *button(const QString &text, const QString &glyph, const QString &tip, bool compact = true)
{
    auto *b = new ChamferButton(text);
    if (!glyph.isEmpty())
        b->setGlyph(glyph);
    b->setCompact(compact);
    b->setToolTip(tip);
    b->setAutoDefault(false);  // Enter must never trigger a comparison action
    return b;
}
}  // namespace

void ComparisonWorkspace::buildUi()
{
    setObjectName(QStringLiteral("comparisonWorkspace"));
    auto *eyebrow = new QLabel(tr("COMPARE"));
    eyebrow->setProperty("role", QStringLiteral("eyebrow"));
    auto *title = new QLabel(tr("Image comparison."));
    title->setFont(Theme::display(22, QFont::DemiBold));

    swap_ = button(tr("Swap"), QStringLiteral("swap"), tr("Swap A and B (Ctrl+Shift+X)"));
    auto *fit = button(tr("Fit both"), QString(), tr("Fit each image to its pane (Ctrl+Shift+F)"));
    auto *actual = button(tr("Actual size"), QString(), tr("Show both at 100 % (Ctrl+Shift+1)"));
    auto *reset = button(tr("Reset zoom"), QStringLiteral("refresh"), tr("Reset zoom and pan (Ctrl+Shift+0)"));
    auto *syncZoom = new ToggleSwitch(tr("Sync zoom"));
    auto *syncPan = new ToggleSwitch(tr("Sync pan"));
    syncZoom->setChecked(true);
    syncPan->setChecked(true);
    syncZoom->setToolTip(tr("Zoom both images together. Off: each image zooms on its own."));
    syncPan->setToolTip(tr("Pan both images together. Off: each image pans on its own."));

    viewer_ = new ComparisonViewer;
    connect(swap_, &QPushButton::clicked, this, &ComparisonWorkspace::swapImages);
    connect(fit, &QPushButton::clicked, viewer_, &ComparisonViewer::fitBoth);
    connect(actual, &QPushButton::clicked, viewer_, &ComparisonViewer::actualSizeBoth);
    connect(reset, &QPushButton::clicked, viewer_, &ComparisonViewer::resetView);
    connect(syncZoom, &QCheckBox::toggled, viewer_, &ComparisonViewer::setSyncZoom);
    connect(syncPan, &QCheckBox::toggled, viewer_, &ComparisonViewer::setSyncPan);

    auto *bar = new QHBoxLayout;
    bar->setSpacing(10);
    bar->addWidget(swap_);
    bar->addWidget(fit);
    bar->addWidget(actual);
    bar->addWidget(reset);
    bar->addSpacing(8);
    bar->addWidget(syncZoom);
    bar->addWidget(syncPan);
    bar->addStretch(1);

    // Per-side header above each pane: tag, filename, facts, and the slot actions.
    for (int s = 0; s < 2; ++s) {
        auto *tag = new QLabel(s == 0 ? QStringLiteral("A") : QStringLiteral("B"));
        tag->setFont(Theme::display(14, QFont::Bold));
        tag->setStyleSheet(QStringLiteral("color:#FF5B2E;"));
        name_[s] = new ElidedLabel;
        name_[s]->setFont(Theme::display(11, QFont::DemiBold));
        name_[s]->setAccessibleName(s == 0 ? tr("Left image name") : tr("Right image name"));
        facts_[s] = new QLabel;
        facts_[s]->setProperty("role", QStringLiteral("muted"));
        facts_[s]->setWordWrap(true);
        replace_[s] = button(tr("Replace…"), QStringLiteral("image"), tr("Choose another image from disk"));
        fromCollection_[s] = button(tr("Collection…"), QStringLiteral("images"), tr("Pick from the imported images"));
        clear_[s] = button(tr("Clear"), QStringLiteral("remove"), tr("Remove this image"));
        const Side side = Side(s);
        connect(replace_[s], &QPushButton::clicked, this, [this, side] { replaceFromDisk(side); });
        connect(fromCollection_[s], &QPushButton::clicked, this, [this, side] { replaceFromCollection(side); });
        connect(clear_[s], &QPushButton::clicked, this, [this, side] { clearImage(side); });

        auto *top = new QHBoxLayout;
        top->setSpacing(8);
        top->addWidget(tag);
        top->addWidget(name_[s], 1);
        auto *actions = new QHBoxLayout;
        actions->setSpacing(8);
        actions->addWidget(replace_[s]);
        actions->addWidget(fromCollection_[s]);
        actions->addWidget(clear_[s]);
        actions->addStretch(1);
        auto *holder = s == 0 ? viewer_->leftHolder() : viewer_->rightHolder();
        auto *hl = static_cast<QVBoxLayout *>(holder->layout());
        hl->insertLayout(0, top);
        hl->insertWidget(1, facts_[s]);
        hl->insertLayout(2, actions);
    }

    // Shortcuts live only while the workspace is visible; none uses Enter or an existing app key.
    auto addKey = [this](const QKeySequence &k, auto fn) {
        auto *sc = new QShortcut(k, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, fn);
    };
    addKey(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X), [this] { swapImages(); });
    addKey(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), [this] { viewer_->fitBoth(); });
    addKey(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_1), [this] { viewer_->actualSizeBoth(); });
    addKey(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_0), [this] { viewer_->resetView(); });

    notice_ = new QLabel;
    notice_->setProperty("role", QStringLiteral("error"));
    notice_->setWordWrap(true);
    notice_->hide();

    table_ = new ComparisonTable;
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(table_);

    auto *split = new QSplitter(Qt::Vertical);
    split->setChildrenCollapsible(false);
    split->addWidget(viewer_);
    split->addWidget(scroll);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    split->setSizes({480, 300});

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(32, 24, 32, 20);
    col->setSpacing(10);
    col->addWidget(eyebrow);
    col->addWidget(title);
    col->addLayout(bar);
    col->addWidget(notice_);
    col->addWidget(split, 1);
}

void ComparisonWorkspace::setNotice(const QString &text)
{
    notice_->setText(text);
    notice_->setVisible(!text.isEmpty());
}

QString ComparisonWorkspace::notice() const
{
    return notice_->text();
}

void ComparisonWorkspace::refreshHeader(Side s)
{
    const QString p = paths_[s];
    name_[s]->setFullText(p.isEmpty() ? tr("No image selected") : QFileInfo(p).fileName());
    clear_[s]->setEnabled(!p.isEmpty());
    QStringList bits;
    if (!p.isEmpty()) {
        const QJsonObject props = props_.value(p);
        const ImageProperties ip = PropertiesModel::fromJson(props);
        const PreviewResult pr = previews_.value(p);
        if (ip.ok()) {
            const QJsonObject d = props.value(QStringLiteral("dimensions")).toObject();
            if (!d.isEmpty())
                bits << PropertiesModel::formatDimensions(d.value(QStringLiteral("width")).toInt(), d.value(QStringLiteral("height")).toInt());
            bits << PropertiesModel::formatBytes(props.value(QStringLiteral("file_size")).toInteger()).section(QStringLiteral("  ("), 0, 0);
        } else if (pr.fullSize.isValid() && pr.ok()) {
            bits << PropertiesModel::formatDimensions(pr.fullSize.width(), pr.fullSize.height());
        }
        if (pr.frames > 1)
            bits << tr("animated: %1 frames, first frame shown").arg(pr.frames);
        if (pr.downscaled)
            bits << tr("preview reduced for speed; zoom % refers to the original");
        if (!props.isEmpty() && !ip.ok())
            bits << tr("%1: %2").arg(ip.status, ip.error);
        if (bits.isEmpty())
            bits << tr("Loading…");
    }
    facts_[s]->setText(bits.join(QStringLiteral("  ·  ")));
}

void ComparisonWorkspace::refresh()
{
    for (int s = 0; s < 2; ++s) {
        const QString p = paths_[s];
        ImagePane *pane = viewer_->pane(s);
        if (p.isEmpty()) {
            if (!shown_[s].isEmpty() || !pane->hasImage())
                pane->setMessage(tr("Select image %1 to compare.").arg(s == 0 ? QStringLiteral("A") : QStringLiteral("B")));
            shown_[s].clear();
        } else if (previews_.contains(p)) {
            if (shown_[s] != p) {
                const PreviewResult &pr = previews_[p];
                if (pr.ok())
                    pane->setPreview(pr.image, pr.fullSize);
                else
                    pane->setMessage(tr("Preview unavailable.\n%1").arg(pr.error), true);
                shown_[s] = p;
            }
        } else if (shown_[s] != p) {
            pane->setMessage(tr("Loading…"));
        }
        refreshHeader(Side(s));
    }
    swap_->setEnabled(!paths_[0].isEmpty() || !paths_[1].isEmpty());

    if (!hasBoth())
        table_->setMessage(tr("Choose two images to compare their properties."));
    else if (props_.contains(paths_[0]) && props_.contains(paths_[1])) {
        comparison_ = ComparisonModel::compare(props_.value(paths_[0]), props_.value(paths_[1]));
        table_->setComparison(comparison_);
    } else {
        comparison_ = Comparison();
        table_->setMessage(tr("Reading image properties…"));
    }
    emit changed();
}

void ComparisonWorkspace::replaceFromDisk(Side side)
{
    const QString f = QFileDialog::getOpenFileName(this, tr("Choose image"), QString(), ImportSelection::dialogFilter());
    if (!f.isEmpty())
        setImage(side, f);
}

void ComparisonWorkspace::replaceFromCollection(Side side)
{
    const QStringList files = collectionFiles();
    if (files.isEmpty()) {
        setNotice(tr("There is no imported collection yet. Import images first, or use Replace… to browse."));
        return;
    }
    FramelessDialog dlg(tr("Choose from collection"), this);
    dlg.setMinimumSize(560, 460);
    auto *filter = new QLineEdit;
    filter->setPlaceholderText(tr("Filter by name…"));
    auto *list = new QListWidget;
    for (const QString &f : files) {
        auto *item = new QListWidgetItem(QFileInfo(f).fileName());
        item->setData(Qt::UserRole, f);
        item->setToolTip(f);
        list->addItem(item);
    }
    connect(filter, &QLineEdit::textChanged, list, [list](const QString &t) {
        for (int i = 0; i < list->count(); ++i)
            list->item(i)->setHidden(!list->item(i)->text().contains(t, Qt::CaseInsensitive));
    });
    auto *ok = button(tr("Use image"), QString(), QString(), false);
    ok->setProperty("variant", QStringLiteral("primary"));
    ok->setEnabled(false);
    connect(list, &QListWidget::currentItemChanged, &dlg, [ok](QListWidgetItem *c) { ok->setEnabled(c != nullptr); });
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);
    dlg.bodyLayout()->addWidget(filter);
    dlg.bodyLayout()->addWidget(list, 1);
    dlg.bodyLayout()->addWidget(ok, 0, Qt::AlignRight);
    if (dlg.exec() == QDialog::Accepted && list->currentItem())
        setImage(side, list->currentItem()->data(Qt::UserRole).toString());
}
