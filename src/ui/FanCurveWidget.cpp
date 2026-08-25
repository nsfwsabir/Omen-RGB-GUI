#include "FanCurveWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QToolTip>
#include <algorithm>
#include <cmath>

FanCurveWidget::FanCurveWidget(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setPoints({{35,12},{50,20},{65,34},{80,52},{95,72}});
}

void FanCurveWidget::setPoints(const QList<FanPoint> &pts) {
    m_points = pts;
    std::sort(m_points.begin(), m_points.end());
    update();
}

QRect FanCurveWidget::plotRect() const {
    int left = 45, right = 15, top = 15, bottom = 30;
    return QRect(left, top, width()-left-right, height()-top-bottom);
}

QPointF FanCurveWidget::dataToPixel(int temp, int pct) const {
    QRect r = plotRect();
    // temp 0-120, pct 0-100
    double x = r.left() + (double)temp / 120.0 * r.width();
    double y = r.bottom() - (double)pct / 100.0 * r.height();
    return QPointF(x,y);
}

FanPoint FanCurveWidget::pixelToData(const QPoint &pos) const {
    QRect r = plotRect();
    double temp = (pos.x() - r.left()) / (double)r.width() * 120.0;
    double pct = (r.bottom() - pos.y()) / (double)r.height() * 100.0;
    FanPoint p{(int)std::round(temp), (int)std::round(pct)};
    clampPoint(p);
    return p;
}

void FanCurveWidget::clampPoint(FanPoint &p) const {
    if (p.temp<0) p.temp=0; if (p.temp>120) p.temp=120;
    if (p.percent<0) p.percent=0; if (p.percent>100) p.percent=100;
}

int FanCurveWidget::hitTest(const QPoint &pos) const {
    const int radius = 8;
    for (int i=0;i<m_points.size();++i) {
        QPointF pf = dataToPixel(m_points[i].temp, m_points[i].percent);
        double dx = pf.x()-pos.x(), dy = pf.y()-pos.y();
        if (dx*dx+dy*dy <= radius*radius) return i;
    }
    return -1;
}

void FanCurveWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().window());

    QRect r = plotRect();
    // background
    p.fillRect(r, QColor(30,30,35));
    // grid
    p.setPen(QPen(QColor(60,60,65),1, Qt::DotLine));
    for (int t=0; t<=120; t+=20) {
        QPointF pt = dataToPixel(t,0);
        p.drawLine(QPointF(pt.x(), r.top()), QPointF(pt.x(), r.bottom()));
    }
    for (int pct=0; pct<=100; pct+=20) {
        QPointF pp = dataToPixel(0,pct);
        p.drawLine(QPointF(r.left(), pp.y()), QPointF(r.right(), pp.y()));
    }
    // axes
    p.setPen(QPen(palette().windowText().color(),1));
    p.drawRect(r);

    // labels
    p.setPen(palette().windowText().color());
    QFont small = font(); small.setPointSize(small.pointSize()-1);
    p.setFont(small);
    for (int t=0; t<=120; t+=20) {
        QPointF pt = dataToPixel(t,0);
        p.drawText(QRectF(pt.x()-15, r.bottom()+2, 30, 16), Qt::AlignCenter, QString::number(t));
    }
    for (int pct=0; pct<=100; pct+=20) {
        QPointF pp = dataToPixel(0,pct);
        p.drawText(QRectF(r.left()-40, pp.y()-8, 35, 16), Qt::AlignRight|Qt::AlignVCenter, QString::number(pct)+"%");
    }
    p.drawText(QRectF(r.left(), r.bottom()+14, r.width(), 16), Qt::AlignCenter, "Temperature °C");
    p.save();
    p.translate(12, r.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-40,-8,80,16), Qt::AlignCenter, "Fan %");
    p.restore();

    // curve line
    if (m_points.size()>=2) {
        QList<FanPoint> sorted = m_points;
        std::sort(sorted.begin(), sorted.end());
        QPainterPath path;
        for (int i=0;i<sorted.size();++i) {
            QPointF pf = dataToPixel(sorted[i].temp, sorted[i].percent);
            if (i==0) path.moveTo(pf);
            else path.lineTo(pf);
        }
        QPen pen(QColor(0,180,255), 2);
        p.setPen(pen);
        p.drawPath(path);

        // fill under curve faint
        QPainterPath fill = path;
        QPointF last = dataToPixel(sorted.last().temp, sorted.last().percent);
        QPointF first = dataToPixel(sorted.first().temp, sorted.first().percent);
        fill.lineTo(last.x(), r.bottom());
        fill.lineTo(first.x(), r.bottom());
        fill.closeSubpath();
        p.fillPath(fill, QColor(0,180,255,30));
    }

    // points
    for (int i=0;i<m_points.size();++i) {
        QPointF pf = dataToPixel(m_points[i].temp, m_points[i].percent);
        QColor col = (i==m_dragIndex) ? QColor(255,200,50) : QColor(0,220,255);
        p.setPen(QPen(col.darker(130),1.5));
        p.setBrush(col);
        p.drawEllipse(pf, 6,6);
        // label temp:pct
        QString lbl = QString("%1:%2").arg(m_points[i].temp).arg(m_points[i].percent);
        p.setPen(palette().windowText().color());
        p.drawText(QRectF(pf.x()-20, pf.y()-22, 40, 12), Qt::AlignCenter, lbl);
    }

    if (m_readOnly) {
        p.setPen(QColor(255,255,255,180));
        p.drawText(rect(), Qt::AlignCenter, "Read-only (max fan active)");
    }
}

void FanCurveWidget::mousePressEvent(QMouseEvent *event) {
    if (m_readOnly) return;
    if (event->button()==Qt::LeftButton) {
        int idx = hitTest(event->pos());
        if (idx!=-1) {
            m_dragIndex = idx;
            emit pointSelected(idx);
            update();
        } else if (m_points.size()<8 && plotRect().contains(event->pos())) {
            // add new point
            FanPoint np = pixelToData(event->pos());
            m_points.append(np);
            std::sort(m_points.begin(), m_points.end());
            emit pointsChanged(m_points);
            update();
        }
    } else if (event->button()==Qt::RightButton) {
        int idx = hitTest(event->pos());
        if (idx!=-1 && m_points.size()>2) {
            m_points.removeAt(idx);
            emit pointsChanged(m_points);
            update();
        }
    }
}

void FanCurveWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_readOnly) return;
    if (m_dragIndex>=0 && (event->buttons() & Qt::LeftButton)) {
        FanPoint np = pixelToData(event->pos());
        // keep sorted order? Allow dragging anywhere but re-sort on release? For smooth, clamp x to neighbours
        // Enforce monotonic temp order by clamping between neighbours
        QList<FanPoint> sorted = m_points;
        std::sort(sorted.begin(), sorted.end());
        // Find current point's sorted position
        // Simpler: just update and resort
        m_points[m_dragIndex] = np;
        emit pointsChanged(m_points);
        update();
    } else {
        int idx = hitTest(event->pos());
        if (idx!=-1) setToolTip(QString("Drag to move, Right-click to remove\n%1°C : %2%").arg(m_points[idx].temp).arg(m_points[idx].percent));
        else if (plotRect().contains(event->pos())) setToolTip("Double-click to add point (max 8)");
        else setToolTip("");
    }
}

void FanCurveWidget::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    if (m_dragIndex!=-1) {
        std::sort(m_points.begin(), m_points.end());
        emit pointsChanged(m_points);
        m_dragIndex=-1;
        update();
    }
}

void FanCurveWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (m_readOnly) return;
    if (plotRect().contains(event->pos()) && m_points.size()<8) {
        FanPoint np = pixelToData(event->pos());
        m_points.append(np);
        std::sort(m_points.begin(), m_points.end());
        emit pointsChanged(m_points);
        update();
    }
}
