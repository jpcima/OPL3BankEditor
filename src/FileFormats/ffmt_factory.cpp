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

#include <memory>
#include <list>

#include "../common.h"

#include "ffmt_factory.h"

#include "format_adlibbnk.h"
#include "format_apogeetmb.h"
#include "format_dmxopl2.h"
#include "format_junlevizion.h"
#include "format_milesopl.h"
#include "format_sb_ibk.h"

typedef std::unique_ptr<FmBankFormatBase> FmBankFormatBase_uptr;
typedef std::list<FmBankFormatBase_uptr>  FmBankFormatsL;

//! Bank formats
static FmBankFormatsL g_formats;
//! Single-Instrument formats
static FmBankFormatsL g_formatsInstr;

static void registerBankFormat(FmBankFormatBase *format)
{
    g_formats.push_back(FmBankFormatBase_uptr(format));
}

static void registerInstFormat(FmBankFormatBase *format)
{
    g_formatsInstr.push_back(FmBankFormatBase_uptr(format));
}

void FmBankFormatFactory::registerAllFormats()
{
    g_formats.clear();
    g_formatsInstr.clear();
    registerBankFormat(new JunleVizion());
    registerBankFormat(new DmxOPL2());
    registerBankFormat(new ApogeeTMB());
    registerBankFormat(new MilesOPL());
    registerBankFormat(new SbIBK_DOS());
    registerInstFormat(new SbIBK_DOS());
    registerBankFormat(new SbIBK_UNIX_READ());
    registerBankFormat(new SbIBK_UNIX2OP_SAVE());
    registerBankFormat(new SbIBK_UNIX4OP_SAVE());
    registerBankFormat(new AdLibAndHmiBnk_reader());
    registerBankFormat(new AdLibBnk_writer());
    registerBankFormat(new HmiBnk_writer());
}



QString FmBankFormatFactory::getSaveFiltersList()
{
    QString formats;
    for(FmBankFormatBase_uptr &p : g_formats)
    {
        Q_ASSERT(p.get());//It must be non-null!
        if(p->formatCaps() & (int)FormatCaps::FORMAT_CAPS_SAVE)
        {
            formats.append(QString("%1 (%2);;").arg(p->formatName()).arg(p->formatExtensionMask()));
        }
    }
    if(formats.endsWith(";;"))
        formats.remove(formats.size()-2, 2);
    return formats;
}

QString FmBankFormatFactory::getOpenFiltersList(bool import)
{
    QString out;
    QString masks;
    QString formats;
    //! Look for importable or openable formats?
    FormatCaps dst = import ?
                FormatCaps::FORMAT_CAPS_IMPORT :
                FormatCaps::FORMAT_CAPS_OPEN;

    for(FmBankFormatBase_uptr &p : g_formats)
    {
        Q_ASSERT(p.get());//It must be non-null!
        if(!masks.isEmpty())
            masks.append(' ');
        if(p->formatCaps() & (int)dst)
        {
            masks.append(p->formatExtensionMask());
            formats.append(QString("%1 (%2);;").arg(p->formatName()).arg(p->formatExtensionMask()));
        }
    }
    out.append(QString("Supported bank files (%1);;").arg(masks));
    out.append(formats);
    out.push_back("All files (*.*)");
    return out;
}

QString FmBankFormatFactory::getInstOpenFiltersList(bool import)
{
    QString out;
    QString masks;
    QString formats;
    //! Look for importable or openable formats?
    FormatCaps dst = import ?
                FormatCaps::FORMAT_CAPS_IMPORT :
                FormatCaps::FORMAT_CAPS_OPEN;
    for(FmBankFormatBase_uptr &p : g_formatsInstr)
    {
        Q_ASSERT(p.get());//It must be non-null!
        if(!masks.isEmpty())
            masks.append(' ');
        if(p->formatCaps() & (int)dst)
        {
            masks.append(p->formatInstExtensionMask());
            formats.append(QString("%1 (%2);;").arg(p->formatInstName()).arg(p->formatInstExtensionMask()));
        }
    }
    out.append(QString("Supported instrument files (%1);;").arg(masks));
    out.append(formats);
    out.push_back("All files (*.*)");
    return out;
}

BankFormats FmBankFormatFactory::getFormatFromFilter(QString filter)
{
    for(FmBankFormatBase_uptr &p : g_formats)
    {
        Q_ASSERT(p.get());//It must be non-null!
        QString f = QString("%1 (%2)").arg(p->formatName()).arg(p->formatExtensionMask());
        if(f == filter)
            return p->formatId();
    }
    return BankFormats::FORMAT_UNKNOWN;
}

QString FmBankFormatFactory::getFilterFromFormat(BankFormats format, int requiredCaps)
{
    for(FmBankFormatBase_uptr &p : g_formats)
    {
        Q_ASSERT(p.get());//It must be non-null!
        if((p->formatId() == format) && (p->formatCaps() & requiredCaps))
            return QString("%1 (%2)").arg(p->formatName()).arg(p->formatExtensionMask());
    }
    return "UNKNOWN";
}

bool FmBankFormatFactory::isImportOnly(BankFormats format)
{
    for(FmBankFormatBase_uptr &p : g_formats)
    {
        Q_ASSERT(p.get());//It must be non-null!
        if(p->formatId() == format)
            return (p->formatCaps() == (int)FormatCaps::FORMAT_CAPS_IMPORT);
    }
    return false;;\
}



FfmtErrCode FmBankFormatFactory::OpenBankFile(QString filePath, FmBank &bank, BankFormats *recent)
{
    char magic[32];
    getMagic(filePath, magic, 32);

    FfmtErrCode err = FfmtErrCode::ERR_UNSUPPORTED_FORMAT;
    BankFormats fmt = BankFormats::FORMAT_UNKNOWN;

    for(FmBankFormatBase_uptr &p : g_formats)
    {
        Q_ASSERT(p.get());//It must be non-null!
        if((p->formatCaps() & (int)FormatCaps::FORMAT_CAPS_OPEN) && p->detect(filePath, magic))
        {
            err = p->loadFile(filePath, bank);
            fmt = p->formatId();
            break;
        }
    }
    if(recent)
        *recent = fmt;

    return err;
}

FfmtErrCode FmBankFormatFactory::ImportBankFile(QString filePath, FmBank &bank, BankFormats *recent)
{
    char magic[32];
    getMagic(filePath, magic, 32);

    FfmtErrCode err = FfmtErrCode::ERR_UNSUPPORTED_FORMAT;
    BankFormats fmt = BankFormats::FORMAT_UNKNOWN;

    for(FmBankFormatBase_uptr &p : g_formats)
    {
        Q_ASSERT(p.get());//It must be non-null!
        if((p->formatCaps() & (int)FormatCaps::FORMAT_CAPS_IMPORT) && p->detect(filePath, magic))
        {
            err = p->loadFile(filePath, bank);
            fmt = p->formatId();
            break;
        }
    }

    if(recent)
        *recent = fmt;
    return err;
}

FfmtErrCode FmBankFormatFactory::SaveBankFile(QString filePath, FmBank &bank, BankFormats dest)
{
    FfmtErrCode err = FfmtErrCode::ERR_UNSUPPORTED_FORMAT;
    for(FmBankFormatBase_uptr &p : g_formats)
    {
        Q_ASSERT(p.get());//It must be non-null!
        if((p->formatCaps() & (int)FormatCaps::FORMAT_CAPS_SAVE) && (p->formatId() == dest))
        {
            err = p->saveFile(filePath, bank);
            break;
        }
    }
    return err;
}

FfmtErrCode FmBankFormatFactory::OpenInstrumentFile(QString filePath,
                                         FmBank::Instrument &ins,
                                         InstFormats *recent,
                                         bool *isDrum)
{
    char magic[32];
    getMagic(filePath, magic, 32);

    FfmtErrCode err = FfmtErrCode::ERR_UNSUPPORTED_FORMAT;
    InstFormats fmt = InstFormats::FORMAT_INST_UNKNOWN;

    for(FmBankFormatBase_uptr &p : g_formatsInstr)
    {
        Q_ASSERT(p.get());//It must be non-null!
        if((p->formatInstCaps() & (int)FormatCaps::FORMAT_CAPS_OPEN) && p->detectInst(filePath, magic))
        {
            err = p->loadFileInst(filePath, ins, isDrum);
            fmt = p->formatInstId();
            break;
        }
    }
    if(recent)
        *recent = fmt;

    return err;
}