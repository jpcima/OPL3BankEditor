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

#ifndef BANK_EDITOR_DELAY_ANALYSIS_H
#define BANK_EDITOR_DELAY_ANALYSIS_H

#include <QDialog>
#include <memory>
#include "bank.h"
#include "opl/measurer.h"
namespace Ui { class DelayAnalysis; }
class QwtPlotCurve;

class DelayAnalysisDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DelayAnalysisDialog(
        const FmBank::Instrument &ins, QWidget *parent = NULL);
    ~DelayAnalysisDialog();

    int exec() override;

protected:
    void changeEvent(QEvent *event) override;

private:
    /**
     * @brief Update the display of measurement results
     */
    void updateDisplay();

    /**
     * @brief Updates the text to display after a language change
     */
    void onLanguageChanged();

private:
    FmBank::Instrument m_ins;
    Measurer *m_measurer;
    Measurer::DurationInfo m_result;
    std::unique_ptr<Ui::DelayAnalysis> m_ui;

    QwtPlotCurve *m_curveOn = nullptr;
    QwtPlotCurve *m_curveOff = nullptr;

    class PlotData;
    PlotData *m_dataOn = nullptr;
    PlotData *m_dataOff = nullptr;
};

#endif // BANK_EDITOR_DELAY_ANALYSIS_H
