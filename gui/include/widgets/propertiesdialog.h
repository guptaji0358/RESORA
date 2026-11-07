#pragma once
// Image properties (Ctrl+I): runs the backend `properties` command for the chosen image(s) and shows
// the result as RESORA section cards. Read-only; nothing is written next to or into the images.
#include <QJsonValue>
#include <QList>
#include <QPointer>
#include <QStringList>

#include "backend/propertiesmodel.h"
#include "widgets/framelessdialog.h"

class BackendProcess;
class ChamferButton;
class QLabel;
class QTemporaryFile;
class QVBoxLayout;
class QWidget;

class PropertiesDialog : public FramelessDialog
{
    Q_OBJECT
public:
    // `files`: images to show first (may be empty: the dialog then asks the user to choose one).
    explicit PropertiesDialog(const QStringList &files, QWidget *parent = nullptr);
    ~PropertiesDialog() override;

    // Show these images (starts the backend). Used by the constructor and "Choose image".
    void setFiles(const QStringList &files);
    // Display an already-parsed result (also what a successful backend run calls).
    void showImages(const QList<ImageProperties> &images);
    int imageCount() const { return images_.size(); }
    int currentIndex() const { return index_; }
    QString currentText() const;
    bool isLoading() const { return job_ != nullptr; }
    // Hosted as an Analyzer page: no "Done" button (the page is never dismissed).
    void setHosted();

signals:
    // Hand-off: the user wants the shown image in Image Comparison.
    void compareRequested(const QString &path);

private:
    void chooseImages();
    void showIndex(int index);
    void showMessage(const QString &text, bool error);
    void clearCards();
    void stopJob();
    void onSucceeded(const QJsonValue &data);
    void onFailed(const QString &code, const QString &message);

    QLabel *title_ = nullptr;
    QLabel *subtitle_ = nullptr;
    QLabel *counter_ = nullptr;
    ChamferButton *prev_ = nullptr;
    ChamferButton *next_ = nullptr;
    ChamferButton *copy_ = nullptr;
    ChamferButton *compare_ = nullptr;
    ChamferButton *done_ = nullptr;
    QWidget *cards_ = nullptr;
    QVBoxLayout *cardsLayout_ = nullptr;

    QPointer<BackendProcess> job_;
    QTemporaryFile *paramsFile_ = nullptr;
    QList<ImageProperties> images_;
    int index_ = -1;
};
