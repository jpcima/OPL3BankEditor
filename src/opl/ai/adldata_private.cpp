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

#include "adldata.h"
#include <math.h>

void cvt_ADLI_to_FMIns(FmBank::Instrument &dst, const adlinsdata &src)
{
    dst = FmBank::emptyInst();

    const adldata *part[2] = {&::adl[src.adlno1], &::adl[src.adlno2]};

    dst.fine_tune = 0;
    double voice2_fine_tune = src.voice2_fine_tune;
    if(voice2_fine_tune != 0)
    {
        if(voice2_fine_tune > 0 && voice2_fine_tune <= 0.000025)
            dst.fine_tune = 1;
        else if(voice2_fine_tune < 0 && voice2_fine_tune >= -0.000025)
            dst.fine_tune = -1;
        else
        {
            long value = static_cast<long>(round(voice2_fine_tune * (1000.0 / 15.625)));
            value = (value < -128) ? -128 : value;
            value = (value > +127) ? +127 : value;
            dst.fine_tune = static_cast<int8_t>(value);
        }
    }

    dst.velocity_offset = src.midi_velocity_offset;
    dst.percNoteNum = src.tone;

    dst.is_blank = (src.flags & adlinsdata::Flag_NoSound) != 0;
    dst.en_4op = (src.flags & adlinsdata::Flag_Real4op) != 0;
    dst.en_pseudo4op = (src.flags & adlinsdata::Flag_Pseudo4op) != 0;
    /* rhythm mode flags TODO */

    for(size_t op = 0; op < 4; op++)
    {
        const adldata &in2op = *part[(op < 2) ? 0 : 1];
        uint32_t regE862 = ((op & 1) == 0) ? in2op.carrier_E862 : in2op.modulator_E862;
        uint8_t reg40 = ((op & 1) == 0) ? in2op.carrier_40 : in2op.modulator_40;
        dst.setWaveForm(op, (uint8_t)(regE862 >> 24));
        dst.setSusRel(op, (uint8_t)(regE862 >> 16));
        dst.setAtDec(op, (uint8_t)(regE862 >> 8));
        dst.setAVEKM(op, (uint8_t)regE862);
        dst.setKSLL(op, reg40);
    }

    dst.note_offset1 = part[0]->finetune;
    dst.setFBConn1(part[0]->feedconn);
    dst.note_offset2 = part[1]->finetune;
    dst.setFBConn2(part[1]->feedconn);

    dst.ms_sound_kon = src.ms_sound_kon;
    dst.ms_sound_koff = src.ms_sound_koff;
}
