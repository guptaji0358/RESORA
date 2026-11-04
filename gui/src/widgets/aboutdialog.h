#pragma once
// Small dark About box: mark, wordmark, tagline, version. Presentation only.
#include <QDialog>

class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
};
