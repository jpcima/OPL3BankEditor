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

#ifndef CUSTOM_OPL3_H
#define CUSTOM_OPL3_H

#include "opl_chip_base.h"

class CustomOPL3 final : public OPLChipBaseBufferedT<CustomOPL3>
{
    void *m_chip;
public:
    //! Opaque type for custom data storage and sharing
    struct ProfileData;

    explicit CustomOPL3(ProfileData &pd);
    ~CustomOPL3() override;

    bool canRunAtPcmRate() const override { return true; }
    void setRate(uint32_t rate) override;
    void reset() override;
    void writeReg(uint16_t addr, uint8_t data) override;
    void writePan(uint16_t addr, uint8_t data) override;
    void nativePreGenerate() override {}
    void nativePostGenerate() override {}
    void nativeGenerateN(int16_t *output, size_t frames) override;
    const char *emulatorName() override;

    /* chip profile editing */

    //! User-defined chip profile
    struct ChipProfile
    {
        const int16_t *wave[8];
    };

    //! Get user profile from chip data
    static void getProfile(ChipProfile &profile, const ProfileData &data);
    //! Set chip data from user profile
    static void setProfile(const ChipProfile &profile, ProfileData &data);

    /* profile storage */

    //! Allocate profile storage and initialize it with defaults
    static ProfileData *newProfileData();
    //! Free profile storage
    static void freeProfileData(ProfileData *pd);

    //! Convenience deleter for RAII
    struct ProfileDeleter
    {
        void operator()(ProfileData *x) { freeProfileData(x); }
    };
};

#endif // CUSTOM_OPL3_H
