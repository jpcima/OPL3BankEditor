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

#include "bank_model.h"
#include "ins_names.h"

class BankModel::ListItem : public QObject
{
public:
    explicit ListItem(QObject *parent = nullptr);
};

//
BankModel::BankModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

BankModel::~BankModel()
{
}

void BankModel::setMode(Mode mode)
{
    beginResetModel();
    m_mode = mode;
    endResetModel();
}

void BankModel::setBank(const FmBank &bank)
{
    beginResetModel();
    m_bank = bank;
    clearItems();
    
    
    endResetModel();
}

int BankModel::rowCount(const QModelIndex &parent) const
{
    return currentItems().size();
}

QVariant BankModel::data(const QModelIndex &index, int role) const
{
    return instrumentName(m_bank, index.row(), m_mode);
}

QString BankModel::instrumentName(const FmBank &bank, unsigned number, Mode mode)
{
    const FmBank::Instrument *ins = (mode == Melodic) ?
        &bank.Ins_Melodic[number] : &bank.Ins_Percussion[number];
    if(ins->name[0] != '\0')
        return QString::fromUtf8(ins->name);
    return (mode == Melodic) ?
        getMidiInsNameM(number) : getMidiInsNameP(number);
}

QVector<BankModel::ListItem *> &BankModel::currentItems()
{
    return (m_mode == Melodic) ? m_melodicItems : m_percussiveItems;
}

const QVector<BankModel::ListItem *> &BankModel::currentItems() const
{
    return const_cast<BankModel *>(this)->currentItems();
}

void BankModel::clearItems()
{
    while(!m_melodicItems.empty())
    {
        delete m_melodicItems.back();
        m_melodicItems.pop_back();
    }
    while(!m_percussiveItems.empty())
    {
        delete m_percussiveItems.back();
        m_percussiveItems.pop_back();
    }
}

//
BankModel::ListItem::ListItem(QObject *parent)
    : QObject(parent)
{
}
