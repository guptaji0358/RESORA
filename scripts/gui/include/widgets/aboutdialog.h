#pragma once
// Small dark About box: mark, wordmark, tagline, version. Presentation only.
#include "widgets/framelessdialog.h"

class AboutDialog : public FramelessDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
};
