/*
 * Interfaces over Yamaha OPL3 (YMF262) chip emulators
 *
 * Copyright (c) 2017-2019 Vitaly Novichkov (Wohlstand)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "custom_opl3.h"
#include "custom/dboplEX.h"
#include <new>
#include <cstdlib>
#include <cstring>
#include <assert.h>

CustomOPL3::CustomOPL3(ProfileData &pd) :
    OPLChipBaseBufferedT(),
    m_chip(new DBOPLEx::Handler((DBOPLEx::ProfileData *)&pd))
{
    reset();
}

CustomOPL3::~CustomOPL3()
{
    DBOPLEx::Handler *chip_r = reinterpret_cast<DBOPLEx::Handler*>(m_chip);
    delete chip_r;
}

void CustomOPL3::setRate(uint32_t rate)
{
    OPLChipBaseBufferedT::setRate(rate);
    DBOPLEx::Handler *chip_r = reinterpret_cast<DBOPLEx::Handler*>(m_chip);
    chip_r->~Handler();
    new(chip_r) DBOPLEx::Handler(chip_r->chip.profile);
    chip_r->Init(effectiveRate());
}

void CustomOPL3::reset()
{
    OPLChipBaseBufferedT::reset();
    DBOPLEx::Handler *chip_r = reinterpret_cast<DBOPLEx::Handler*>(m_chip);
    chip_r->~Handler();
    new(chip_r) DBOPLEx::Handler(chip_r->chip.profile);
    chip_r->Init(effectiveRate());
}

void CustomOPL3::writeReg(uint16_t addr, uint8_t data)
{
    DBOPLEx::Handler *chip_r = reinterpret_cast<DBOPLEx::Handler*>(m_chip);
    chip_r->WriteReg(static_cast<Bit32u>(addr), data);
}

void CustomOPL3::writePan(uint16_t addr, uint8_t data)
{
    DBOPLEx::Handler *chip_r = reinterpret_cast<DBOPLEx::Handler*>(m_chip);
    chip_r->WritePan(static_cast<Bit32u>(addr), data);
}

void CustomOPL3::nativeGenerateN(int16_t *output, size_t frames)
{
    DBOPLEx::Handler *chip_r = reinterpret_cast<DBOPLEx::Handler*>(m_chip);
    Bitu frames_i = frames;
    chip_r->GenerateArr(output, &frames_i);
}

void CustomOPL3::getProfile(ChipProfile &profile, const ProfileData &data)
{
    const DBOPLEx::ProfileData &pd = *(const DBOPLEx::ProfileData *)&data;

    for (unsigned w = 0; w < 8; ++w)
        profile.wave[w] = &pd.WaveTable[w * 1024];
}

void CustomOPL3::setProfile(const ChipProfile &profile, ProfileData &data)
{
    DBOPLEx::ProfileData &pd = *(DBOPLEx::ProfileData *)&data;

    for (unsigned w = 0; w < 8; ++w) {
        const int16_t *table = profile.wave[w];
        table = table ? table : &DBOPLEx::WaveROM[w * 1024];
        std::memcpy(&pd.WaveTable[w * 1024], table, 1024 * sizeof(int16_t));
    }
}

CustomOPL3::ProfileData *CustomOPL3::newProfileData()
{
    DBOPLEx::ProfileData *pd = new DBOPLEx::ProfileData;
    std::memcpy(pd->WaveTable, DBOPLEx::WaveROM, sizeof(DBOPLEx::WaveROM));
    return (CustomOPL3::ProfileData *)pd;
}

void CustomOPL3::freeProfileData(ProfileData *pd)
{
    delete (DBOPLEx::ProfileData *)pd;
}

const char *CustomOPL3::emulatorName()
{
    return "Custom DOSBox OPL3";
}
