#pragma once
// Base of the Image Comparison visuals: a QPainter canvas that shows one aspect of InsightMetrics.
// Handles the shared parts (data, unavailable state, accessibility text, A/B colours, helpers);
// subclasses only say whether they have data and how to paint it. Presentation only.
#include <QColor>
#include <QWidget>

#include "backend/insightmetrics.h"

class InsightCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit InsightCanvas(const QString &accessibleName, int minHeight, QWidget *parent = nullptr);

    void setMetrics(const InsightMetrics &m);
    const InsightMetrics &metrics() const { return m_; }
    // True when the canvas draws real data; false when it shows the "unavailable" message.
    bool showsData() const { return hasData(); }
    QString unavailableMessage() const;

    static QColor colorA();   // ember
    static QColor colorB();   // cool steel blue; A/B are always also labelled in text
    static QString sideName(Side s);

protected:
    virtual bool hasData() const = 0;
    virtual QString describe() const = 0;                      // accessible description of the data
    virtual QString missingText() const = 0;                   // why nothing can be drawn
    virtual void paintContent(QPainter &p, const QRect &area) = 0;

    void paintEvent(QPaintEvent *) override;

    // Single-line text, elided with "…" to fit.
    static void drawElided(QPainter &p, const QRect &r, int flags, const QString &text);

    // One labelled horizontal bar (three lines: "A  name .... value", the bar on a shared track,
    // a sub line). `fraction` is 0..1 of the shared scale; <0 means no value (track only, muted).
    static constexpr int kBarBlockHeight = 52;
    static void drawBarBlock(QPainter &p, const QRect &r, const QColor &color, const QString &tag, const QString &name,
                             const QString &value, double fraction, const QString &sub, bool emphasise);

    InsightMetrics m_;
};
