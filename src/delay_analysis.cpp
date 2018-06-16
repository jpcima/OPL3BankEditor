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

#include "delay_analysis.h"
#include "ui_delay_analysis.h"
#include <qwt/qwt_plot.h>
#include <qwt/qwt_plot_curve.h>
#include <qwt/qwt_point_data.h>
#include <qwt/qwt_spline.h>

DelayAnalysisDialog::DelayAnalysisDialog(const FmBank::Instrument &ins, QWidget *parent)
    : QDialog(parent), m_ins(ins),
      m_measurer(new Measurer(this)), m_ui(new Ui::DelayAnalysis)
{
    m_ui->setupUi(this);
}

DelayAnalysisDialog::~DelayAnalysisDialog()
{
}

int DelayAnalysisDialog::exec()
{
    if (!m_measurer->doComputation(m_ins, m_result))
        return QDialog::Rejected;

    updateDisplay();

    return QDialog::exec();
}

class DelayAnalysisDialog::PlotData : public QwtSyntheticPointData
{
    const double m_step;
    const std::vector<double> &m_data;
    mutable QPolygonF m_splinebuf;
public:
    PlotData(double xstep, const std::vector<double> &data);
    size_t size() const override;
    double x(uint index) const override;
    double y(double x) const override;
};

void DelayAnalysisDialog::updateDisplay()
{
    const Measurer::DurationInfo &result = m_result;

    Ui::DelayAnalysis &ui = *m_ui;

    double xMaxOn = result.amps_timestep * result.amps_on.size();
    double yMaxOn = 0;
    for(double y : result.amps_on)
        yMaxOn = (y > yMaxOn) ? y : yMaxOn;

    double xMaxOff = result.amps_timestep * result.amps_off.size();
    double yMaxOff = 0;
    for(double y : result.amps_off)
        yMaxOff = (y > yMaxOff) ? y : yMaxOff;

    QwtPlot *plotOn = ui.plotDelayOn;
    plotOn->setAxisScale(QwtPlot::xBottom, 0.0, xMaxOn);
    plotOn->setAxisScale(QwtPlot::yLeft, 0.0, yMaxOn);

    QwtPlot *plotOff = ui.plotDelayOff;
    plotOff->setAxisScale(QwtPlot::xBottom, 0.0, xMaxOff);
    plotOff->setAxisScale(QwtPlot::yLeft, 0.0, yMaxOff);

    QwtPlotCurve *curveOn = m_curveOn;
    if(!curveOn) {
        curveOn = m_curveOn = new QwtPlotCurve("On");
        curveOn->attach(plotOn);
    }

    QwtPlotCurve *curveOff = m_curveOff;
    if(!curveOff) {
        curveOff = m_curveOff = new QwtPlotCurve("Off");
        curveOff->attach(plotOff);
    }

    PlotData *dataOn = new PlotData(result.amps_timestep, result.amps_on);
    delete m_dataOn;
    m_dataOn = dataOn;
    curveOn->setData(dataOn);

    PlotData *dataOff = new PlotData(result.amps_timestep, result.amps_off);
    delete m_dataOff;
    m_dataOff = dataOff;
    curveOff->setData(dataOff);

    plotOn->replot();
    plotOff->replot();
}

void DelayAnalysisDialog::onLanguageChanged()
{
    m_ui->retranslateUi(this);
}

void DelayAnalysisDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        onLanguageChanged();
    QDialog::changeEvent(event);
}

DelayAnalysisDialog::PlotData::PlotData(double xstep, const std::vector<double> &data)
    : QwtSyntheticPointData(data.size()),
      m_step(xstep), m_data(data)
{
}

size_t DelayAnalysisDialog::PlotData::size() const
{
    return m_data.size();
}

double DelayAnalysisDialog::PlotData::x(uint index) const
{
    return index * m_step;
}

double DelayAnalysisDialog::PlotData::y(double x) const
{
    QPolygonF &poly = m_splinebuf;
    poly.clear();
    long index = (long)x;
    for(long i = 0; i < 8; ++i)
    {
        long i1 = ((i & 1) == 0) ?
            (index + (1 + i / 2)) : (index - (1 + i / 2));
        QPointF p(i1 * m_step, 0);
        if(i1 >= 0)
        {
            if ((size_t)i1 >= m_data.size()) continue;
            p.setY(m_data[i1]);
        }
        poly.push_back(p);
    }

    QwtSpline spline;
    spline.setPoints(poly);
    return spline.value(x);
}
