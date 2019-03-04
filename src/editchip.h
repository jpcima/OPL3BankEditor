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

#ifndef EDITCHIP_H
#define EDITCHIP_H

#include "opl/chips/custom_opl3.h"
#include <QDialog>
#include <memory>

namespace Ui { class ChipEditor; }
class QAbstractButton;

class ChipEditor : public QDialog
{
    Q_OBJECT

public:
    explicit ChipEditor(QWidget *parent = nullptr);
    ~ChipEditor();

    void clearChipProfile();
    const CustomOPL3::ChipProfile &chipProfile() const { return m_chipProfile; }

signals:
    void editingFinished();

private slots:
    void on_buttonBox_clicked(QAbstractButton *button);
    void on_valWaveNumber_valueChanged(int value);
    void regenWave();
    void onChangeWave();
    void onTriggeredGeneratorAction();

private:
    void updateWaveDisplay();
    bool computeWave(int16_t *out, unsigned length, const QString &waveCode, double phaseDistort);
    static double distortPhase(double phase, double amt);

    static CustomOPL3::ChipProfile defaultChipProfile();

    void initGenerators();
    void initWaveCode(unsigned w);

    void showError(const QString &error);

    CustomOPL3::ChipProfile m_chipProfile;
    std::unique_ptr<Ui::ChipEditor> m_ui;

    struct WaveOptions
    {
        bool phaseDist = false;
        double phaseDistAmount = 0.0;
    };

    QString m_waveCode[8];
    WaveOptions m_waveOpts[8];

    const int16_t *m_waveDefault[8];
    int16_t m_waveCurrent[8 * 1024];
};

#endif
