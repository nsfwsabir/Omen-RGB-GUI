#pragma once
#include <QWidget>
#include <QList>
#include "core/FanController.h"

class FanCurveWidget : public QWidget {
    Q_OBJECT
public:
    explicit FanCurveWidget(QWidget *parent = nullptr);

    void setPoints(const QList<FanPoint> &pts);
    QList<FanPoint> points() const { return m_points; }

    void setReadOnly(bool ro) { m_readOnly = ro; update(); }
    QSize sizeHint() const override { return QSize(480, 260); }

signals:
    void pointsChanged(const QList<FanPoint> &pts);
    void pointSelected(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QList<FanPoint> m_points;
    int m_dragIndex = -1;
    bool m_readOnly = false;

    QRect plotRect() const;
    QPointF dataToPixel(int temp, int pct) const;
    FanPoint pixelToData(const QPoint &pos) const;
    int hitTest(const QPoint &pos) const; // returns index or -1
    void clampPoint(FanPoint &p) const;
};
