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

#include "editchip.h"
#include "ui_editchip.h"
#include <QAbstractButton>
#include <QAction>
#include <QMenu>
#include <QPainter>
#include <QDebug>
#include <lua.hpp>
#include <string.h>
#include <math.h>

ChipEditor::ChipEditor(QWidget *parent)
    : QDialog(parent),
      m_ui(new Ui::ChipEditor)
{
    m_ui->setupUi(this);

    // obtain default profile information (waves)
    CustomOPL3::ChipProfile dp = defaultChipProfile();
    for (unsigned w = 0; w < 8; ++w)
        m_waveDefault[w] = dp.wave[w];

    //
    initGenerators();

    //
    connect(m_ui->valWaveCode, SIGNAL(textChanged()), this, SLOT(regenWave()));
    connect(m_ui->chkPhaseDist, SIGNAL(toggled(bool)), this, SLOT(regenWave()));
    connect(m_ui->valPhaseDist, SIGNAL(valueChanged(int)), this, SLOT(regenWave()));

    //
    clearChipProfile();

    //
    onChangeWave();
}

void ChipEditor::clearChipProfile()
{
    CustomOPL3::ChipProfile profile = defaultChipProfile();

    memset(&m_chipProfile, 0, sizeof(m_chipProfile));

    for(unsigned w = 0; w < 8; ++w)
    {
        initWaveCode(w);
        m_waveOpts[w] = WaveOptions();
        memcpy(&m_waveCurrent[w * 1024], profile.wave[w], 1024 * sizeof(int16_t));
        m_chipProfile.wave[w] = &m_waveCurrent[w * 1024];
    }

    onChangeWave();
}

ChipEditor::~ChipEditor()
{
}

void ChipEditor::on_buttonBox_clicked(QAbstractButton *button)
{
    QDialogButtonBox::ButtonRole role = m_ui->buttonBox->buttonRole(button);

    if(role == QDialogButtonBox::ApplyRole)
        emit editingFinished();
}

void ChipEditor::on_valWaveNumber_valueChanged(int value)
{
    Q_UNUSED(value);
    onChangeWave();
}

void ChipEditor::regenWave()
{
    unsigned w = m_ui->valWaveNumber->value();
    m_waveCode[w] = m_ui->valWaveCode->toPlainText();
    m_waveOpts[w].phaseDist = m_ui->chkPhaseDist->isChecked();
    m_waveOpts[w].phaseDistAmount = m_ui->valPhaseDist->value() * 1e-2;
    computeWave(&m_waveCurrent[w * 1024], 1024, m_waveCode[w],
        m_waveOpts[w].phaseDist ? m_waveOpts[w].phaseDistAmount : 0);
    updateWaveDisplay();
}

void ChipEditor::onChangeWave()
{
    unsigned w = m_ui->valWaveNumber->value();

    m_ui->valWaveCode->blockSignals(true);
    m_ui->valWaveCode->setPlainText(m_waveCode[w]);
    m_ui->valWaveCode->blockSignals(false);

    m_ui->chkPhaseDist->blockSignals(true);
    m_ui->chkPhaseDist->setChecked(m_waveOpts[w].phaseDist);
    m_ui->chkPhaseDist->blockSignals(false);

    m_ui->valPhaseDist->blockSignals(true);
    m_ui->valPhaseDist->setValue(m_waveOpts[w].phaseDist * 100);
    m_ui->valPhaseDist->blockSignals(false);

    regenWave();
}

void ChipEditor::onTriggeredGeneratorAction()
{
    QAction *act = qobject_cast<QAction *>(sender());
    m_ui->valWaveCode->setPlainText(act->data().toString());
}

bool ChipEditor::computeWave(int16_t *out, unsigned length, const QString &waveCode, double phaseDistort)
{
    memset(out, 0, 1024 * sizeof(int16_t));

    ///
    struct LuaDeleter {
        void operator()(lua_State *x) { lua_close(x); }
    };

    auto l_alloc = [](void *ud, void *ptr, size_t osize, size_t nsize) -> void *
    {
        (void)ud; (void)osize;
        if(nsize == 0) { free(ptr); return nullptr; }
        return realloc(ptr, nsize);
    };

    std::unique_ptr<lua_State, LuaDeleter> L;
    L.reset(lua_newstate(+l_alloc, nullptr));
    luaL_openlibs(L.get());

    if (luaL_dostring(L.get(), waveCode.toUtf8().constData())) {
        showError(lua_tostring(L.get(), -1));
        return false;
    }

    ///
    for(unsigned i = 0; i < 1024; ++i)
    {
        int gen = lua_getglobal(L.get(), "wave");
        if (gen == 0) {
            lua_pop(L.get(), 1);
            showError(tr("the function \"wave\" is not defined"));
            return false;
        }

        double phase = (double)i / 1024;
        phase = distortPhase(phase, phaseDistort);

        lua_pushnumber(L.get(), phase);

        if(lua_pcall(L.get(), 1, 1, 0) != 0)
        {
            showError(QString::fromUtf8(lua_tostring(L.get(), -1)));
            return false;
        }

        int isnumber;
        double sample = lua_tonumberx(L.get(), -1, &isnumber);
        lua_pop(L.get(), 1);
        if(!isnumber)
        {
            showError(tr("the result is not a number"));
            return false;
        }

        int isample = (int)lround(sample * 4085);
        isample = (isample < +4085) ? isample : +4085;
        isample = (isample > -4085) ? isample : -4085;
        out[i] = (int16_t)isample;
    }

    showError("");
    return true;
}

double ChipEditor::distortPhase(double phase, double amt)
{
    phase = phase * 2 - 1;

    if(amt > 0) // positive distortion
    {
        double amin = 0.5;
        double amax = 5;
        double a = amin + amt * (amax - amin);
        double p = tanh(phase * a);
        phase = p / fabs(tanh(-a));
    }
    else if(amt < 0) // negative distortion
    {
        amt = -16 * amt;

        auto g = [](double x, double a) -> double
                     { return pow(2, -a * (1 - x)); };
        double g0 = g(0, amt);
        double g1 = g(1, amt);
        double p = (g(fabs(phase), amt) - g0) / (g1 - g0);
        phase = (phase < 0) ? -p : +p;
    }

    phase = (phase + 1) * 0.5;

    return phase;
}

void ChipEditor::updateWaveDisplay()
{
    unsigned w = m_ui->valWaveNumber->value();
    m_ui->waveDefault->setData(m_waveDefault[w], 1024);
    m_ui->waveCurrent->setData(&m_waveCurrent[w * 1024], 1024);
}

CustomOPL3::ChipProfile ChipEditor::defaultChipProfile()
{
    CustomOPL3::ChipProfile profile;
    static std::unique_ptr<CustomOPL3::ProfileData, CustomOPL3::ProfileDeleter> data(CustomOPL3::newProfileData());
    CustomOPL3::getProfile(profile, *data);
    return profile;
}

static QPair<const char *, const char *> generators[] =
{
    {"function wave(p)\n"
     "  return math.sin(p * 2 * math.pi)\n"
     "end\n", "Sine"},

    {"function wave(p)\n"
     "  if p > 0.5 then return 0 end\n"
     "  return math.sin(p * 2 * math.pi)\n"
     "end\n", "Half-Sine"},

    {"function wave(p)\n"
     "  return math.abs(math.sin(p * 2 * math.pi))\n"
     "end\n", "Absolute Sine"},

    {"function wave(p)\n"
     "  if (math.floor(p * 4) & 1) == 1 then return 0 end\n"
     "  return math.abs(math.sin(p * 2 * math.pi))\n"
     "end\n", "Pulse-Sine"},

    {"function wave(p)\n"
     "  if p > 0.5 then return 0 end\n"
     "  return math.sin(p * 4 * math.pi)\n"
     "end\n", "Sine - even periods only"},

    {"function wave(p)\n"
     "  if p > 0.5 then return 0 end\n"
     "  return math.abs(math.sin(p * 4 * math.pi))\n"
     "end\n", "Abs-Sine - even periods only"},

    {"function wave(p)\n"
     "  if p < 0.5 then return 1 else return -1 end\n"
     "end\n", "Square"},

    {"function wave(p)\n"
     "  local q = math.abs(0.5 - p)\n"
     "  local s = math.pow(2, - q * 32)\n"
     "  if p < 0.5 then s = -s end\n"
     "  return s\n"
     "end\n", "Derived Square"},

    // Extra

    {"function wave(p)\n"
     "  return p * 2 - 1\n"
     "end\n", "Ramp"},

    {"function wave(p)\n"
     "  return 1 - p * 2\n"
     "end\n", "Saw"},

    {"function wave(p)\n"
     "  local duty = 0.25\n"
     "  if p < duty then return 1 else return -1 end\n"
     "end\n", "Pulse"},

    {"function wave(p)\n"
     "  local s = 4 * p\n"
     "  if p > 0.75 then s = s - 4\n"
     "  elseif p > 0.25 then s = 2 - s end\n"
     "  return s\n"
     "end\n", "Triangle"},

    {"function wave(p)\n"
     "  local n = math.floor(p * 4)\n"
     "  local q = p - 0.25 * n\n"
     "  local s = math.sin((3 - n) * math.pi / 2 + q * 2 * math.pi)\n"
     "  if p < 0.5 then s = s + 1 else s = s - 1 end\n"
     "  return s\n"
     "end\n", "Spike"},

    {"function wave(p)\n"
     "  local duty = 0.75\n"
     "  local s\n"
     "  if p < duty then s = math.sin(p / duty * 0.5 * math.pi)\n"
     "  else s = 1 - math.sin((p - duty) / (1-duty) * 0.5 * math.pi) end\n"
     "  return s * 2 - 1\n"
     "end\n", "Charge"},
};

void ChipEditor::initGenerators()
{
    unsigned i = 0;
    for(const QPair<const char *, const char *> gen : generators)
    {
        QAction *act = new QAction(gen.second);
        QToolButton *btn = (i < 8) ? m_ui->btnBasicWave : m_ui->btnExtraWave;
        btn->addAction(act);
        act->setData(gen.first);

        QPixmap iconPixmap(32, 32);
        QPainter iconPainter(&iconPixmap);
        int16_t data[1024];
        computeWave(data, 1024, gen.first, 0);
        showError("");
        WaveView::paintIntoRect(iconPainter, iconPixmap.rect(), data, 1024, 12);
        act->setIcon(iconPixmap);

        connect(act, SIGNAL(triggered()), this, SLOT(onTriggeredGeneratorAction()));
        ++i;
    }

    m_ui->btnBasicWave->setPopupMode(QToolButton::InstantPopup);
    m_ui->btnExtraWave->setPopupMode(QToolButton::InstantPopup);

    for(unsigned w = 0; w < 8; ++w)
        initWaveCode(w);
}

void ChipEditor::initWaveCode(unsigned w)
{
    m_waveCode[w] = generators[w].first;
}

void ChipEditor::showError(const QString &error)
{
    m_ui->txtCodeErrors->setPlainText(error);
    m_ui->txtCodeErrors->setVisible(!error.isEmpty());
}
