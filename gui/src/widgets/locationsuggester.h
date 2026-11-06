#pragma once
// Path recommendation UI for ONE line edit: when the edit is focused / clicked / typed in, shows a
// RESORA-styled popup of matching saved locations (see LocationRecommender). Choosing one only fills
// the edit; it never starts a scan or writes a file. Attach it only to path inputs.
#include <QObject>

#include "locations/locationrecommender.h"

class LocationStore;
class QFrame;
class QLineEdit;
class QListWidget;

class LocationSuggester : public QObject
{
    Q_OBJECT
public:
    LocationSuggester(QLineEdit *edit, const LocationStore *store, const LocationRecommender::Context &context);

    void showSuggestions();  // refresh from the store + current text; hides when there is nothing to offer
    void hideSuggestions();
    bool isPopupVisible() const;
    int rowCount() const;
    QStringList values() const;  // what choosing each row would put into the edit
    void choose(int row);

signals:
    void chosen(const QString &value);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuild();
    void place();
    void moveSelection(int delta);

    QLineEdit *edit_;
    const LocationStore *store_;
    LocationRecommender::Context context_;
    QFrame *popup_;
    QListWidget *list_;
};
