/*
 * OPL Bank Editor by Wohlstand, a free tool for music bank editing
 * Copyright (c) 2018-2019 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "waveview.h"
#include <QPainter>
#include <QDebug>

WaveView::WaveView(QWidget *parent)
    : QWidget(parent)
{
}

void WaveView::setData(const int16_t *data, unsigned length)
{
    m_data.assign(data, data + length);
    repaint();
}

void WaveView::paintIntoRect(
    QPainter &p, const QRectF &r,
    const int16_t *data, int length, unsigned bitResolution)
{
    qreal w = r.width();
    qreal h = r.height();

    p.fillRect(r, Qt::black);

    QPen penAxis(Qt::blue);
    p.setPen(penAxis);
    p.drawLine(r.topLeft() + QPointF(0.25 * w, 0),
               r.bottomLeft() + QPointF(0.25 * w, 0));
    p.drawLine(r.topRight() - QPointF(0.25 * w, 0),
               r.bottomRight() - QPointF(0.25 * w, 0));
    p.drawLine(r.topLeft() + QPointF(0, 0.25 * h),
               r.topRight() + QPointF(0, 0.25 * h));
    p.drawLine(r.bottomLeft() - QPointF(0, 0.25 * h),
               r.bottomRight() - QPointF(0, 0.25 * h));
    penAxis.setWidthF(1.5);
    p.setPen(penAxis);
    p.drawLine(QPointF(0.5 * w, r.top()), QPointF(0.5 * w, r.bottom()));
    p.drawLine(QPointF(r.left(), 0.5 * h), QPointF(r.right(), 0.5 * h));

    if (length <= 0)
        return;

    QPen penCurve(Qt::red, 3.0);
    p.setPen(penCurve);

    int step = qMax(1, (int)(length * (1 / w)));
    qreal scaleFactor = 1.0 / (1u << bitResolution);

    qreal sample1 = data[0] * scaleFactor;
    qreal x1 = 0;
    qreal y1 = h * (1 - (sample1 + 1) * 0.5);
    for(int i = 1; i < length; i += step)
    {
        i = (i < length) ? i : (length - 1);
        qreal ri = i * (1.0 / length);

        qreal sample2 = data[i] * scaleFactor;
        qreal x2 = ri * w;
        qreal y2 = h * (1 - (sample2 + 1) * 0.5);
        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));

        sample1 = sample2;
        x1 = x2;
        y1 = y2;
    }
}

void WaveView::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    paintIntoRect(p, rect(), m_data.data(), (int)m_data.size(), m_bitResolution);
}
