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

#ifndef WAVEVIEW_H
#define WAVEVIEW_H

#include <QWidget>
#include <vector>

class WaveView : public QWidget
{
    Q_OBJECT

public:
    explicit WaveView(QWidget *parent = nullptr);

    void setData(const int16_t *data, unsigned length);

    static void paintIntoRect(
        QPainter &p, const QRectF &r,
        const int16_t *data, int length, unsigned bitResolution);

protected:
    void paintEvent(QPaintEvent *event);

private:
    unsigned m_bitResolution = 12;
    std::vector<int16_t> m_data;
};

#endif // WAVEVIEW_H
