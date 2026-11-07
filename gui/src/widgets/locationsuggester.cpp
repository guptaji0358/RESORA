#include "widgets/locationsuggester.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include "locations/locationstore.h"
#include "ui/theme.h"

namespace {
constexpr int kRowHeight = 46;
constexpr int kMaxRows = 6;
constexpr int kPathRole = Qt::UserRole + 1;
constexpr int kValueRole = Qt::UserRole + 2;

// Two lines per row: name (display font) over its folder (mono, middle-elided); ember bar on the
// highlighted row.
class RowDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {100, kRowHeight}; }
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
    {
        const bool on = opt.state & QStyle::State_Selected;
        p->fillRect(opt.rect, on ? QColor(0x24, 0x1b, 0x16) : Theme::panel());
        if (on)
            p->fillRect(QRect(opt.rect.left(), opt.rect.top(), 3, opt.rect.height()), Theme::ember());
        const QRect r = opt.rect.adjusted(14, 6, -12, -6);
        p->setPen(Theme::paper());
        p->setFont(Theme::display(10.5, QFont::DemiBold, 0.4));
        p->drawText(QRect(r.left(), r.top(), r.width(), 18), Qt::AlignVCenter | Qt::AlignLeft,
                    index.data(Qt::DisplayRole).toString());
        p->setPen(Theme::mute());
        p->setFont(Theme::mono(8.5));
        const QString path = index.data(kPathRole).toString();
        p->drawText(QRect(r.left(), r.top() + 20, r.width(), 16), Qt::AlignVCenter | Qt::AlignLeft,
                    QFontMetrics(p->font()).elidedText(path, Qt::ElideMiddle, r.width()));
    }
};
}  // namespace

LocationSuggester::LocationSuggester(QLineEdit *edit, const LocationStore *store,
                                     const LocationRecommender::Context &context)
    : QObject(edit), edit_(edit), store_(store), context_(context),
      popup_(new QFrame(edit, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus)),
      list_(new QListWidget)
{
    popup_->setObjectName(QStringLiteral("locationPopup"));
    popup_->setAttribute(Qt::WA_ShowWithoutActivating);
    popup_->setFocusPolicy(Qt::NoFocus);
    popup_->setStyleSheet(QStringLiteral("QFrame#locationPopup{background:#161412;border:1px solid #4A443D;}"));
    auto *col = new QVBoxLayout(popup_);
    col->setContentsMargins(1, 1, 1, 1);
    col->setSpacing(0);
    auto *caption = new QLabel(tr("SAVED LOCATIONS"));
    caption->setProperty("role", QStringLiteral("caption"));
    caption->setContentsMargins(14, 7, 14, 5);
    col->addWidget(caption);

    list_->setFocusPolicy(Qt::NoFocus);
    list_->setFrameShape(QFrame::NoFrame);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setItemDelegate(new RowDelegate(list_));
    list_->setMouseTracking(true);
    list_->setStyleSheet(QStringLiteral("QListWidget{background:#161412;border:none;}"));
    col->addWidget(list_);

    edit->installEventFilter(this);
    connect(edit, &QLineEdit::textEdited, this, [this] { showSuggestions(); });
    connect(list_, &QListWidget::itemEntered, this, [this](QListWidgetItem *i) { list_->setCurrentItem(i); });
    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem *i) { choose(list_->row(i)); });
    if (store_)
        connect(store_, &LocationStore::changed, this, [this] {
            if (isPopupVisible())
                showSuggestions();
        });
}

bool LocationSuggester::isPopupVisible() const { return popup_->isVisible(); }
int LocationSuggester::rowCount() const { return list_->count(); }

QStringList LocationSuggester::values() const
{
    QStringList v;
    for (int i = 0; i < list_->count(); ++i)
        v << list_->item(i)->data(kValueRole).toString();
    return v;
}

void LocationSuggester::rebuild()
{
    list_->clear();
    if (!store_ || !edit_->isEnabled() || edit_->isReadOnly())
        return;
    for (const auto &rec : LocationRecommender::recommend(store_->locations(), edit_->text(), context_)) {
        auto *item = new QListWidgetItem(rec.location.name, list_);
        item->setData(kPathRole, rec.location.displayPath());
        item->setData(kValueRole, rec.value);
        item->setToolTip(rec.location.displayPath());
    }
}

void LocationSuggester::showSuggestions()
{
    rebuild();
    if (list_->count() == 0) {
        hideSuggestions();
        return;
    }
    edit_->window()->installEventFilter(this);  // the edit is parented by now; hide when the window moves
    place();
    popup_->show();
}

void LocationSuggester::place()
{
    const int rows = qMin(list_->count(), kMaxRows);
    list_->setFixedHeight(rows * kRowHeight);
    popup_->setFixedWidth(qMax(edit_->width(), 320));
    popup_->adjustSize();
    popup_->move(edit_->mapToGlobal(QPoint(0, edit_->height() + 2)));
}

void LocationSuggester::hideSuggestions()
{
    popup_->hide();
}

void LocationSuggester::choose(int row)
{
    if (row < 0 || row >= list_->count())
        return;
    const QString value = list_->item(row)->data(kValueRole).toString();
    hideSuggestions();
    edit_->setText(value);  // not textEdited: choosing must not reopen the popup
    edit_->setCursorPosition(edit_->text().size());
    emit chosen(value);
}

void LocationSuggester::moveSelection(int delta)
{
    const int n = list_->count();
    if (n == 0)
        return;
    const int cur = list_->currentRow();
    list_->setCurrentRow(cur < 0 ? (delta > 0 ? 0 : n - 1) : (cur + delta + n) % n);
}

bool LocationSuggester::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != edit_) {  // owning window moved / resized / lost activation: the popup would float loose
        if (event->type() == QEvent::Move || event->type() == QEvent::Resize || event->type() == QEvent::WindowDeactivate)
            hideSuggestions();
        return false;
    }
    switch (event->type()) {
    case QEvent::FocusIn:
        showSuggestions();
        break;
    case QEvent::FocusOut:
    case QEvent::Hide:
    case QEvent::EnabledChange:
        hideSuggestions();
        break;
    case QEvent::MouseButtonPress:
        if (!isPopupVisible())
            showSuggestions();
        break;
    case QEvent::KeyPress: {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Down || key == Qt::Key_Up) {
            if (!isPopupVisible())
                showSuggestions();
            else
                moveSelection(key == Qt::Key_Down ? 1 : -1);
            return true;
        }
        if ((key == Qt::Key_Return || key == Qt::Key_Enter) && isPopupVisible() && list_->currentRow() >= 0) {
            choose(list_->currentRow());
            return true;
        }
        if (key == Qt::Key_Escape && isPopupVisible()) {
            hideSuggestions();
            return true;
        }
        if (key == Qt::Key_Tab || key == Qt::Key_Backtab)
            hideSuggestions();
        break;
    }
    default:
        break;
    }
    return false;
}
