#pragma once
// Analytics view (phase 04): collection and resolution readouts, resolution ladder, orientation,
// formats + storage, and the images behind the extremes (with Properties / folder shortcuts).
// Shows what the engine returned for the *current* imported collection; it never starts a scan
// itself (Refresh only asks MainWindow to). Presentation only: no statistic is computed here.
#include <QHash>
#include <QJsonObject>
#include <QWidget>

#include "backend/analyticsmodel.h"

class AnalyticsDashboard;
class ChamferButton;
class QStackedWidget;
class SegmentedSwitch;
class MetricCard;
class PixelSpan;
class QFrame;
class QLabel;
class ShareChart;
class QVBoxLayout;
class ExtremeRow;

class AnalyticsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AnalyticsPanel(QWidget *parent = nullptr);

    void clear();
    // `result` is the data of a `scan` or `analytics` result (source info + "analytics").
    void showAnalytics(const QJsonObject &result);
    // Restored .resora dashboard: result() keeps the persisted JSON unchanged; the extreme-image rows use
    // `pathByRole` (largest/smallest/highest/lowest -> resolved file). Fails (panel untouched) if a role that
    // has an image has no resolved path; never falls back to the serialized path.
    // `removedRoles`: associations the user removed; those rows show "Removed from dashboard" and need no path.
    bool showRestored(const QJsonObject &result, const QHash<QString, QString> &pathByRole, QString *error,
                      const QStringList &removedRoles = {});
    const QStringList &removedRoles() const { return removedRoles_; }
    // 0 = Dashboard (default), 1 = Detailed View. Switching never rescans.
    int viewIndex() const;
    void setViewIndex(int index);
    bool hasData() const { return data_.valid; }
    const AnalyticsData &data() const { return data_; }
    // The result object last passed to showAnalytics() (empty after clear()): the serializable state
    // of this dashboard together with viewIndex(). Used by the .resora adapter (backend/dashboardstate.h).
    const QJsonObject &result() const { return result_; }
    QString sourceText() const;

    // The imported collection changed since these numbers were made: show a hint instead of silently
    // presenting numbers for other images.
    void setStale(bool stale);
    bool isStale() const;
    void setRefreshEnabled(bool enabled);

signals:
    void refreshRequested();
    void exportRequested();  // Export .resora
    void openRequested();    // Open .resora
    void propertiesRequested(const QString &path);

private:
    void populate();
    void present(const QJsonObject &result, const AnalyticsData &d);
    void showFailures();
    QString describeSource(const QJsonObject &result) const;
    void revealFolderOf(const QString &path) const;

    AnalyticsData data_;
    QJsonObject result_;
    QStringList removedRoles_;
    QString source_;
    QLabel *emptyArt_, *empty_, *sourceLabel_, *staleLabel_, *basis_;
    QWidget *content_, *detailed_;
    AnalyticsDashboard *dashboard_;
    QStackedWidget *views_;
    SegmentedSwitch *viewSwitch_;
    ChamferButton *refresh_, *export_ = nullptr, *openFile_ = nullptr;
    QFrame *failBanner_;
    QLabel *failText_, *failList_;
    ChamferButton *failToggle_;
    ExtremeRow *largest_, *smallest_, *highest_, *lowest_;
    PixelSpan *span_;
    ShareChart *categories_, *orientation_, *formats_;
    QLabel *otherFormats_;
};
