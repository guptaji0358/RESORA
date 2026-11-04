#pragma once
// Scan summary (metric cards, resolution table) + confirmed report locations with Open buttons.
#include <QJsonObject>
#include <QWidget>

class ElidedLabel;
class MetricCard;
class QLabel;
class QPushButton;
class DistributionBar;
class QVBoxLayout;

class ResultsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsPanel(QWidget *parent = nullptr);

    void clear();
    // `data` is the scan result envelope data. wantCsv/wantTxt say what was requested, so a
    // requested report the backend did not confirm is shown as not generated.
    void showResult(const QJsonObject &data, bool wantCsv, bool wantTxt);

private:
    struct ReportRow
    {
        QWidget *box = nullptr;
        QLabel *caption = nullptr;
        ElidedLabel *path = nullptr;
        QPushButton *open = nullptr;
        QPushButton *folder = nullptr;
        QString file;
    };
    void addReportRow(ReportRow &row, const QString &title, QVBoxLayout *into);
    void setReport(ReportRow &row, bool wanted, const QString &confirmedPath);
    void openPath(const QString &path) const;

    QLabel *empty_;
    QWidget *content_;
    MetricCard *total_;
    MetricCard *analyzed_;
    MetricCard *failed_;
    QLabel *skipped_;
    QLabel *tableHeading_;
    DistributionBar *byResolution_;
    ReportRow csv_;
    ReportRow txt_;
};
