#pragma once
// Relinking table (Phase 6): ALL linked images that failed validation are listed at once with their
// dashboard role, status and actions (Choose Replacement / Clear / Remove from Dashboard). Choices are
// proposals only: the import engine re-checks size and SHA-256 of every image before anything opens, and
// still blocks on every image that is neither verified nor explicitly removed. State lives in RelinkModel.
#include <QWidget>
#include <functional>

#include "backend/relinkmodel.h"
#include "backend/resoraworkflow.h"

class QLabel;
class QTableWidget;

class RelinkPanel : public QWidget
{
    Q_OBJECT
public:
    explicit RelinkPanel(QWidget *parent = nullptr);

    // First call: every failing image appears at once. Later calls fold in the engine's verdict.
    void setProblems(const QList<resora::LinkStatus> &problems);
    void reset();  // forget everything (a different file is being opened)
    bool assign(const QString &imageId, const QString &path);
    void assignFolder(const QString &folder);  // unresolved rows only, by file name, recursive
    bool clearReplacement(const QString &imageId);
    // Asks for confirmation (see setRemoveConfirmer), then removes the dashboard association for this open.
    // The image file on disk is never touched. Returns false when declined or unknown.
    bool requestRemove(const QString &imageId);
    // Tests replace the modal question: receives the image name and the roles it would be removed from.
    void setRemoveConfirmer(std::function<bool(const QString &, const QStringList &)> f) { confirm_ = std::move(f); }

    const resoraui::RelinkModel &model() const { return model_; }
    QHash<QString, QString> chosen() const { return model_.relinks(); }
    QSet<QString> omitted() const { return model_.omitted(); }
    bool complete() const { return model_.complete(); }
    int rowCount() const { return model_.rows().size(); }
    QTableWidget *table() const { return table_; }

signals:
    void changed();

private:
    void locate(const QString &imageId);
    void rebuild();

    resoraui::RelinkModel model_;
    std::function<bool(const QString &, const QStringList &)> confirm_;
    QLabel *summary_, *detail_;
    QTableWidget *table_;
};
