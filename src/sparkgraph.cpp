// SPDX-License-Identifier: GPL-3.0-or-later
#include "sparkgraph.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

SparkGraph::SparkGraph(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &SparkGraph::changed, this, &SparkGraph::recompute);
}

static QVector<double> toVec(const QVariantList &l)
{
    QVector<double> v;
    v.reserve(l.size());
    for (const QVariant &x : l)
        v.append(x.toDouble());
    return v;
}

// Round the automatic scale up to a "nice" number (1, 2, 5 x 10^n)
static double niceCeil(double v)
{
    if (v <= 0)
        return 1;
    const double e = std::pow(10.0, std::floor(std::log10(v)));
    const double f = v / e;
    const double n = f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10;
    return n * e;
}

void SparkGraph::recompute()
{
    m_d1 = toVec(m_values);
    m_d2 = toVec(m_values2);
    double mx = m_maxValue;
    if (mx <= 0) {
        mx = m_minAutoMax;
        for (double v : std::as_const(m_d1))
            mx = std::max(mx, v);
        for (double v : std::as_const(m_d2))
            mx = std::max(mx, v);
        mx = niceCeil(mx);
    }
    if (mx != m_effMax) {
        m_effMax = mx;
        Q_EMIT effectiveMaxChanged();
    }
    update();
}

void SparkGraph::drawSeries(QPainter *p, const QRectF &r, const QVector<double> &v, const QColor &c) const
{
    if (v.size() < 2)
        return;
    const int cap = std::max(2, m_capacity);
    const double step = r.width() / double(cap - 1);
    const int n = std::min(int(v.size()), cap);
    const int start = int(v.size()) - n;
    QPainterPath line;
    double firstX = 0;
    for (int i = 0; i < n; ++i) {
        const double x = r.right() - double(n - 1 - i) * step;
        const double frac = std::clamp(v[start + i] / m_effMax, 0.0, 1.0);
        const double y = r.bottom() - frac * (r.height() - m_lineWidth) - m_lineWidth / 2;
        if (i == 0) {
            line.moveTo(x, y);
            firstX = x;
        } else {
            line.lineTo(x, y);
        }
    }
    QPainterPath fill = line;
    fill.lineTo(r.right(), r.bottom());
    fill.lineTo(firstX, r.bottom());
    fill.closeSubpath();
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    QColor top = c;
    top.setAlphaF(0.32f);
    QColor bottom = c;
    bottom.setAlphaF(0.04f);
    g.setColorAt(0, top);
    g.setColorAt(1, bottom);
    p->fillPath(fill, g);
    QPen pen(c, m_lineWidth);
    pen.setJoinStyle(Qt::RoundJoin);
    p->setPen(pen);
    p->drawPath(line);
}

void SparkGraph::paint(QPainter *p)
{
    const QRectF r = boundingRect();
    if (r.width() < 4 || r.height() < 4)
        return;
    p->setRenderHint(QPainter::Antialiasing, true);
    if (m_grid) {
        p->setPen(QPen(m_gridColor, 1));
        for (int i = 1; i < 4; ++i) {
            const double y = std::round(r.top() + r.height() * i / 4.0) + 0.5;
            p->drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        }
    }
    drawSeries(p, r, m_d2, m_color2);
    drawSeries(p, r, m_d1, m_color);
}
