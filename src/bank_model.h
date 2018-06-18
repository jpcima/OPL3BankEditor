/*
 * OPL Bank Editor by Wohlstand, a free tool for music bank editing
 * Copyright (c) 2018 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifndef BANK_MODEL_H
#define BANK_MODEL_H

#include "bank.h"
#include <QAbstractListModel>
#include <QVector>

class BankModel : public QAbstractListModel
{
public:
    explicit BankModel(QObject *parent = nullptr);
    ~BankModel();

    enum Mode { Melodic, Percussive };

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);

    const FmBank &bank() const { return m_bank; }
    void setBank(const FmBank &bank);

    // reimplemented
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    static QString instrumentName(const FmBank &bank, unsigned number, Mode mode);
    Mode m_mode = Melodic;
    FmBank m_bank;

    class ListItem;
    QVector<ListItem *> &currentItems();
    const QVector<ListItem *> &currentItems() const;
    void clearItems();
    QVector<ListItem *> m_melodicItems;
    QVector<ListItem *> m_percussiveItems;
};

#endif // BANK_MODEL_H
