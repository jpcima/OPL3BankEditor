/*
 * OPL Bank Editor by Wohlstand, a free tool for music bank editing
 * Copyright (c) 2016-2017 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifndef ADLIBBNK_H
#define ADLIBBNK_H

#include "ffmt_base.h"

class AdLibAndHmiBnk_reader final : public FmBankFormatBase
{
    BankFormats m_recentFormat = BankFormats::FORMAT_UNKNOWN;
public:
    AdLibAndHmiBnk_reader();
    ~AdLibAndHmiBnk_reader() = default;

    bool detect(const QString &filePath, char* magic) override;
    FfmtErrCode  loadFile(QString filePath, FmBank &bank) override;
    int  formatCaps() override;
    QString formatName() override;
    QString formatExtensionMask() override;
    BankFormats formatId() override;
};

class AdLibBnk_writer final : public FmBankFormatBase
{
public:
    AdLibBnk_writer();
    ~AdLibBnk_writer() = default;

    FfmtErrCode saveFile(QString filePath, FmBank &bank) override;
    int  formatCaps() override;
    QString formatName() override;
    QString formatExtensionMask() override;
    BankFormats formatId() override;
};

class HmiBnk_writer final : public FmBankFormatBase
{
public:
    HmiBnk_writer();
    ~HmiBnk_writer() = default;

    FfmtErrCode  saveFile(QString filePath, FmBank &bank) override;
    int  formatCaps() override;
    QString formatName() override;
    QString formatExtensionMask() override;
    BankFormats formatId() override;
};

#endif // ADLIBBNK_H
