#pragma once
// RESORA-styled confirmation (frameless, dark). The safe choice is the focus default and Enter never
// confirms a destructive action unless the user tabbed to it. Used before overwriting a .resora file.
#include "widgets/framelessdialog.h"

class ConfirmDialog : public FramelessDialog
{
    Q_OBJECT
public:
    ConfirmDialog(const QString &title, const QString &text, const QString &confirmLabel, bool danger,
                  QWidget *parent = nullptr);
    static bool ask(QWidget *parent, const QString &title, const QString &text, const QString &confirmLabel,
                    bool danger = true);
};
