#pragma once
// Lists EVERY linked image that failed validation with its recorded path and reason, and lets the user pick
// a replacement file for each (one by one or by searching a folder). Choices are proposals only: the import
// engine re-checks size and SHA-256 of all images before anything opens.
#include <QHash>
#include <QWidget>

#include "backend/resoraworkflow.h"

class QLabel;
class QVBoxLayout;

class RelinkPanel : public QWidget
{
    Q_OBJECT
public:
    explicit RelinkPanel(QWidget *parent = nullptr);

    void setProblems(const QList<resora::LinkStatus> &problems);
    const QList<resora::LinkStatus> &problems() const { return problems_; }
    void assign(const QString &imageId, const QString &path);
    void assignFolder(const QString &folder);  // by file name, recursive; reports how many matched
    const QHash<QString, QString> &chosen() const { return chosen_; }
    bool complete() const { return resoraui::allAssigned(problems_, chosen_); }
    int rowCount() const { return problems_.size(); }

signals:
    void changed();

private:
    void locate(const QString &imageId);
    void rebuild();

    QList<resora::LinkStatus> problems_;
    QHash<QString, QString> chosen_;
    QLabel *summary_;
    QVBoxLayout *rows_;
};
