#pragma once
// Header lockup: painted RESORA mark + wordmark (RES in paper, ORA in ember) + descriptor.
#include <QWidget>

class BrandLockup : public QWidget
{
    Q_OBJECT
public:
    explicit BrandLockup(QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
};
