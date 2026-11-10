// SPDX-License-Identifier: GPL-3.0-or-later
// A small, fast line/area graph for live history values.
#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QVector>
#include <QtQml/qqmlregistration.h>

class SparkGraph : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList values MEMBER m_values NOTIFY changed)
    Q_PROPERTY(QVariantList values2 MEMBER m_values2 NOTIFY changed)
    Q_PROPERTY(QColor color MEMBER m_color NOTIFY changed)
    Q_PROPERTY(QColor color2 MEMBER m_color2 NOTIFY changed)
    Q_PROPERTY(QColor gridColor MEMBER m_gridColor NOTIFY changed)
    Q_PROPERTY(double maxValue MEMBER m_maxValue NOTIFY changed)        // 0 = scale automatically
    Q_PROPERTY(double minAutoMax MEMBER m_minAutoMax NOTIFY changed)    // floor for automatic scale
    Q_PROPERTY(int capacity MEMBER m_capacity NOTIFY changed)
    Q_PROPERTY(bool grid MEMBER m_grid NOTIFY changed)
    Q_PROPERTY(double lineWidth MEMBER m_lineWidth NOTIFY changed)
    Q_PROPERTY(double effectiveMax READ effectiveMax NOTIFY effectiveMaxChanged)
public:
    explicit SparkGraph(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
    double effectiveMax() const { return m_effMax; }

Q_SIGNALS:
    void changed();
    void effectiveMaxChanged();

private:
    void recompute();
    void drawSeries(QPainter *p, const QRectF &r, const QVector<double> &v, const QColor &c) const;

    QVariantList m_values, m_values2;
    QVector<double> m_d1, m_d2;
    QColor m_color = QColor(0x3d, 0xae, 0xe9);
    QColor m_color2 = QColor(0xf6, 0x74, 0x00);
    QColor m_gridColor = QColor(128, 128, 128, 40);
    double m_maxValue = 0;
    double m_minAutoMax = 1;
    double m_effMax = 1;
    int m_capacity = 120;
    bool m_grid = true;
    double m_lineWidth = 1.6;
};
