/*
 * OPL Bank Editor by Wohlstand, a free tool for music bank editing
 * Copyright (c) 2016-2018 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifndef IS_QT_4
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#endif
#include <QQueue>
#include <QProgressDialog>

#include <vector>
#include <chrono>
#include <cmath>
#include <cassert>
#include <memory>

#include "measurer.h"

//Measurer is always needs for emulator
#include "chips/opl_chip_base.h"
#include "chips/nuked_opl3.h"
#include "chips/nuked_opl3_v174.h"
#include "chips/dosbox_opl3.h"

struct DurationInfo
{
    uint64_t    peak_amplitude_time;
    double      peak_amplitude_value;
    double      quarter_amplitude_time;
    double      begin_amplitude;
    double      interval;
    double      keyoff_out_time;
    int64_t     ms_sound_kon;
    int64_t     ms_sound_koff;
    bool        nosound;
    uint8_t     padding[7];
};

struct NoteInfo
{
    unsigned block;
    unsigned fnum;
};

struct EnvelopeInfo
{
    NoteInfo note;
    int ar;
    int dr;
    int sl;
    int rr;
    int tl;
    bool nts;
    bool ksr;
    int ksl;
    bool sustained;
    unsigned wave;
};

static unsigned effective_rate(
    unsigned rate, unsigned ksr, unsigned nts, unsigned fnum, unsigned block)
{
    unsigned effective_rate = 4 * rate;
    if (!ksr)
        effective_rate += block >> 1;
    else
        effective_rate += (block << 1) | ((fnum >> (9 - nts)) & 1);
    return effective_rate;
}

static double wave_rms(unsigned wave)
{
    static constexpr double opl3_wave_rms[8] = {
        0.70561416881800276, 0.38471346311423527,
        0.30705309587280949, 0.38471346311423527,
        0.49798248543363655, 0.38426129383516933,
        0.99767965006992609, 0.21419520618172677,
    };
    return opl3_wave_rms[wave & 7];
}

static double level_modifier(NoteInfo note, int tl, int ksl)
{
    // ksl tables from nuked
    static const uint8_t kslrom[16] =
        { 0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64 };
    static const uint8_t kslshift[4] =
        { 8, 1, 2, 0 };

    int tli = tl << 2;
    int ksli = (kslrom[note.fnum >> 6] << 2) - ((8 - note.block) << 5);
    ksli = (ksli > 0) ? ksli : 0;
    ksli >>= kslshift[ksl];
    return (tli + ksli) / 512.0;
}

static double solve_attack(
    double vrms, const EnvelopeInfo egp[], unsigned egcount)
{
    // attack phase approx: E(t,r) = 1-exp(-a*b^(r+c)*t)
    static constexpr double attack_a = 1.149779557179130,
                            attack_b = 1.189578077537087,
                            attack_c = -1.771203939904738;

    constexpr unsigned egmax = 6; /* 2 notes, 3 carriers */
    assert(egcount <= egmax);

    // precompute the constant rate argument
    double arg[egmax];
    // and the level modifier
    double lmod[egmax];
    for (unsigned i = 0; i < egcount; ++i) {
        unsigned eff_rate = effective_rate(egp[i].ar, egp[i].ksr, egp[i].nts, egp[i].note.fnum, egp[i].note.block);
        arg[i] = -attack_a * std::pow(attack_b, eff_rate + attack_c);
        lmod[i] = level_modifier(egp[i].note, egp[i].tl, egp[i].ksl);
    }

    // evaluator of total rms level at time t
    auto evaluate =
        [&arg, &lmod, &egp, egcount](double t) -> double
            {
                double v = 0.0;
                for (unsigned i = 0; i < egcount; ++i) {
                    // compute basic envelope
                    double e = 1.0 - std::exp(t * arg[i]);
                    // apply level modifications
                    e -= lmod[i];
                    e = (e > 0) ? e : 0;
                    // compute rms modulated with envelope
                    double w = e * wave_rms(egp[i].wave);
                    v += w * w;
                }
                // compute rms total
                return std::sqrt(v);
            };

    // find t by binary search
    double t1 = 0.0, t2 = 10.0;
    double t = (t2 + t1) * 0.5;
    constexpr unsigned iterations = 16;  /* increase for precision */
    for (unsigned i = 0; i < iterations; ++i) {
        double v = evaluate(t);
        if (vrms < v) { t2 = t; }
        else { t1 = t; }
        t = (t2 + t1) * 0.5;
    }
    return t;
}

static double solve_release(
    double vrms, const EnvelopeInfo egp[], unsigned egcount)
{
    // release phase approx: E(t,r) = 1-a*b^(r+c)*t
    static constexpr double release_a = 0.010612748520266,
                            release_b = 1.185551946574785,
                            release_c = 1.013799170930869;

    constexpr unsigned egmax = 6; /* 2 notes, 3 carriers */
    assert(egcount <= egmax);

    // precompute the constant rate argument
    double arg[egmax];
    // and the level modifier
    double lmod[egmax];
    for (unsigned i = 0; i < egcount; ++i) {
        unsigned eff_rate = effective_rate(egp[i].ar, egp[i].ksr, egp[i].nts, egp[i].note.fnum, egp[i].note.block);
        arg[i] = -release_a * std::pow(release_b, eff_rate + release_c);
        lmod[i] = (egp[i].sustained) ? ((egp[i].sl << 4) / 512.0) : 0.0;
        lmod[i] += level_modifier(egp[i].note, egp[i].tl, egp[i].ksl);
    }

    // evaluator of total rms level at time t
    auto evaluate =
        [&arg, &lmod, &egp, egcount](double t) -> double
            {
                double v = 0.0;
                for (unsigned i = 0; i < egcount; ++i) {
                    // compute basic envelope
                    double e = 1.0 + t * arg[i];
                    // apply level modifications
                    e -= lmod[i];
                    e = (e > 0) ? e : 0;
                    // compute rms modulated with envelope
                    double w = e * wave_rms(egp[i].wave);
                    v += w * w;
                }
                // compute rms total
                return std::sqrt(v);
            };

    // find t by binary search
    double t1 = 0.0, t2 = 50.0;
    double t = (t2 + t1) * 0.5;
    constexpr unsigned iterations = 16;  /* increase for precision */
    for (unsigned i = 0; i < iterations; ++i) {
        double v = evaluate(t);
        if (vrms > v) { t2 = t; }
        else { t1 = t; }
        t = (t2 + t1) * 0.5;
    }
    return t;
}

static double solve_release_faster(
    double vrms, const EnvelopeInfo egp[], unsigned egcount)
{
    // release phase approx: E(t,r) = 1-a*b^(r+c)*t
    static constexpr double release_a = 0.010612748520266,
                            release_b = 1.185551946574785,
                            release_c = 1.013799170930869;

    double poly[3] = {- vrms * vrms, 0, 0};

    // compute second degree polynomial
    for (unsigned i = 0; i < egcount; ++i) {
        unsigned eff_rate = effective_rate(egp[i].ar, egp[i].ksr, egp[i].nts, egp[i].note.fnum, egp[i].note.block);
        double r = release_a * std::pow(release_b, eff_rate + release_c);
        double lmod = (egp[i].sustained) ? ((egp[i].sl << 4) / 512.0) : 0.0;
        lmod += level_modifier(egp[i].note, egp[i].tl, egp[i].ksl);
        double v = 1.0 - lmod;
        v = (v > 0) ? v : 0;
        double w = wave_rms(egp[i].wave);
        poly[0] += v * v * w * w;
        poly[1] += -2 * r * v * w * w;
        poly[2] += r * r * w * w;
    }

    // solve t
    double delta = poly[1] * poly[1] - 4 * poly[2] * poly[0];  // b^2-4ac
    if (delta < 0)  // no real solutions
        return HUGE_VAL;
    delta = std::sqrt(delta);
    double sol1 = (-poly[1] + delta) / (2 * poly[2]);
    double sol2 = (-poly[1] - delta) / (2 * poly[2]);
    return std::max(sol1, sol2);
}

static void MeasureDurations(FmBank::Instrument *in_p, OPLChipBase *chip)
{
    FmBank::Instrument &in = *in_p;
    std::vector<int16_t> stereoSampleBuf;

    const unsigned rate = 44100;
    const unsigned interval             = 150;
    const unsigned samples_per_interval = rate / interval;
    const int notenum = in.percNoteNum >= 128 ? (in.percNoteNum - 128) : in.percNoteNum;

#define WRITE_REG(key, value) opl->writeReg(key, value)
    OPLChipBase *opl = chip;

    static const short initdata[(2 + 3 + 2 + 2) * 2] =
    {
        0x004, 96, 0x004, 128,      // Pulse timer
        0x105, 0, 0x105, 1, 0x105, 0, // Pulse OPL3 enable, leave disabled
        0x001, 32, 0x0BD, 0         // Enable wave & melodic
    };

    opl->setRate(rate);

    for(unsigned a = 0; a < 18; a += 2)
        WRITE_REG((uint16_t)initdata[a], (uint8_t)initdata[a + 1]);

    const unsigned n_notes = in.en_4op || in.en_pseudo4op ? 2 : 1;
    enum { max_notes = 2 };

    unsigned x[2] = {0, 0};
    if(n_notes == 2 && !in.en_pseudo4op)
    {
        WRITE_REG(0x105, 1);
        WRITE_REG(0x104, 1);
    }

    uint8_t rawData[2][11];

    rawData[0][0] = in.getAVEKM(MODULATOR1);
    rawData[0][1] = in.getAVEKM(CARRIER1);
    rawData[0][2] = in.getAtDec(MODULATOR1);
    rawData[0][3] = in.getAtDec(CARRIER1);
    rawData[0][4] = in.getSusRel(MODULATOR1);
    rawData[0][5] = in.getSusRel(CARRIER1);
    rawData[0][6] = in.getWaveForm(MODULATOR1);
    rawData[0][7] = in.getWaveForm(CARRIER1);
    rawData[0][8] = in.getKSLL(MODULATOR1);
    rawData[0][9] = in.getKSLL(CARRIER1);
    rawData[0][10] = in.getFBConn1();

    rawData[1][0] = in.getAVEKM(MODULATOR2);
    rawData[1][1] = in.getAVEKM(CARRIER2);
    rawData[1][2] = in.getAtDec(MODULATOR2);
    rawData[1][3] = in.getAtDec(CARRIER2);
    rawData[1][4] = in.getSusRel(MODULATOR2);
    rawData[1][5] = in.getSusRel(CARRIER2);
    rawData[1][6] = in.getWaveForm(MODULATOR2);
    rawData[1][7] = in.getWaveForm(CARRIER2);
    rawData[1][8] = in.getKSLL(MODULATOR2);
    rawData[1][9] = in.getKSLL(CARRIER2);
    rawData[1][10] = in.getFBConn2();

    for(unsigned n = 0; n < n_notes; ++n)
    {
        static const unsigned char patchdata[11] =
        {0x20, 0x23, 0x60, 0x63, 0x80, 0x83, 0xE0, 0xE3, 0x40, 0x43, 0xC0};
        for(unsigned a = 0; a < 10; ++a)
            WRITE_REG(patchdata[a] + n * 8, rawData[n][a]);
        WRITE_REG(patchdata[10] + n * 8, rawData[n][10] | 0x30);
    }

    NoteInfo notes[max_notes];

    for(unsigned n = 0; n < n_notes; ++n)
    {
        NoteInfo &note = notes[n];

        double hertz = 172.00093 * std::exp(0.057762265 * (notenum + in.fine_tune));
        if(hertz > 131071)
        {
            std::fprintf(stderr, "MEASURER WARNING: Why does note %d + finetune %d produce hertz %g?          \n",
                         notenum, in.fine_tune, hertz);
            hertz = 131071;
        }
        x[n] = 0x2000;

        note.block = 0;
        while(hertz >= 1023.5)
        {
            hertz /= 2.0;    // Calculate octave
            ++note.block;
        }
        x[n] += note.block * 0x400;

        note.fnum = (unsigned int)(hertz + 0.5);
        x[n] += note.fnum;

        // Keyon the note
        WRITE_REG(0xA0 + n * 3, x[n] & 0xFF);
        WRITE_REG(0xB0 + n * 3, x[n] >> 8);
    }

    enum { max_algorithms = 6, max_carriers = 3 };

    unsigned algorithm = in.getFBConn1() & 1;
    if(in.en_4op || in.en_pseudo4op)
        algorithm = 2 + (algorithm | ((in.getFBConn2() & 1) << 1));

    uint8_t carriers[max_carriers];
    unsigned num_carriers = 0;

    switch(algorithm)
    {
    case 0:
        carriers[num_carriers++] = MODULATOR1;
        carriers[num_carriers++] = CARRIER1;
        break;
    case 1:
        carriers[num_carriers++] = CARRIER1;
        break;
    case 2:
        carriers[num_carriers++] = CARRIER2;
        break;
    case 3:
        carriers[num_carriers++] = MODULATOR1;
        carriers[num_carriers++] = CARRIER2;
        break;
    case 4:
        carriers[num_carriers++] = CARRIER1;
        carriers[num_carriers++] = CARRIER2;
        break;
    case 5:
        carriers[num_carriers++] = MODULATOR1;
        carriers[num_carriers++] = MODULATOR2;
        carriers[num_carriers++] = CARRIER2;
        break;
    }

    EnvelopeInfo envelopes[max_carriers * max_notes];
    for(unsigned i = 0; i < num_carriers * n_notes; ++i)
    {
        EnvelopeInfo &env = envelopes[i];
        env.note = notes[i / num_carriers];
        unsigned op = carriers[i];
        env.ar = in.OP[op].attack;
        env.dr = in.OP[op].decay;
        env.sl = in.OP[op].sustain;
        env.rr = in.OP[op].release;
        env.tl = in.OP[op].level;
        env.nts = false;  // is this good?
        env.ksr = in.OP[op].ksr;
        env.ksl = in.OP[op].ksl;
        env.sustained = in.OP[op].eg;
        env.wave = in.OP[op].waveform;
    }

    // TODO it's not correct level to check for
    double solver_attack_time = solve_attack(0.2, envelopes, num_carriers);
    double solver_release_time = solve_release(0.2, envelopes, num_carriers);
    double solver_release2_time = solve_release_faster(0.2, envelopes, num_carriers);

    const unsigned max_silent = 6;
    const unsigned max_on  = 40;
    const unsigned max_off = 60;

    // For up to 40 seconds, measure mean amplitude.
    std::vector<double> amplitudecurve_on;
    double highest_sofar = 0;
    short sound_min = 0, sound_max = 0;
    for(unsigned period = 0; period < max_on * interval; ++period)
    {
        stereoSampleBuf.clear();
        stereoSampleBuf.resize(samples_per_interval * 2, 0);

        opl->generate(stereoSampleBuf.data(), samples_per_interval);

        double mean = 0.0;

        for(unsigned long c = 0; c < samples_per_interval; ++c)
        {
            short s = stereoSampleBuf[c * 2];
            mean += s;
            if(sound_min > s) sound_min = s;
            if(sound_max < s) sound_max = s;
        }
        mean /= samples_per_interval;
        double std_deviation = 0;
        for(unsigned long c = 0; c < samples_per_interval; ++c)
        {
            double diff = (stereoSampleBuf[c * 2] - mean);
            std_deviation += diff * diff;
        }
        std_deviation = std::sqrt(std_deviation / samples_per_interval);
        amplitudecurve_on.push_back(std_deviation);
        if(std_deviation > highest_sofar)
            highest_sofar = std_deviation;

        if((period > max_silent * interval) &&
            ((std_deviation < highest_sofar * 0.2)||
             (sound_min >= -1 && sound_max <= 1))
        )
            break;
    }

    // Keyoff the note
    for(unsigned n = 0; n < n_notes; ++n)
        WRITE_REG(0xB0 + n * 3, (x[n] >> 8) & 0xDF);

    // Now, for up to 60 seconds, measure mean amplitude.
    std::vector<double> amplitudecurve_off;
    for(unsigned period = 0; period < max_off * interval; ++period)
    {
        stereoSampleBuf.clear();
        stereoSampleBuf.resize(samples_per_interval * 2);

        opl->generate(stereoSampleBuf.data(), samples_per_interval);

        double mean = 0.0;
        for(unsigned long c = 0; c < samples_per_interval; ++c)
        {
            short s = stereoSampleBuf[c * 2];
            mean += s;
            if(sound_min > s) sound_min = s;
            if(sound_max < s) sound_max = s;
        }
        mean /= samples_per_interval;
        double std_deviation = 0;
        for(unsigned long c = 0; c < samples_per_interval; ++c)
        {
            double diff = (stereoSampleBuf[c * 2] - mean);
            std_deviation += diff * diff;
        }
        std_deviation = std::sqrt(std_deviation / samples_per_interval);
        amplitudecurve_off.push_back(std_deviation);

        if(std_deviation < highest_sofar * 0.2)
            break;

        if((period > max_silent * interval) && (sound_min >= -1 && sound_max <= 1))
            break;
    }

    /* Analyze the results */
    double begin_amplitude        = amplitudecurve_on[0];
    double peak_amplitude_value   = begin_amplitude;
    size_t peak_amplitude_time    = 0;
    size_t quarter_amplitude_time = amplitudecurve_on.size();
    size_t keyoff_out_time        = 0;

    for(size_t a = 1; a < amplitudecurve_on.size(); ++a)
    {
        if(amplitudecurve_on[a] > peak_amplitude_value)
        {
            peak_amplitude_value = amplitudecurve_on[a];
            peak_amplitude_time  = a;
        }
    }
    for(size_t a = peak_amplitude_time; a < amplitudecurve_on.size(); ++a)
    {
        if(amplitudecurve_on[a] <= peak_amplitude_value * 0.2)
        {
            quarter_amplitude_time = a;
            break;
        }
    }
    for(size_t a = 0; a < amplitudecurve_off.size(); ++a)
    {
        if(amplitudecurve_off[a] <= peak_amplitude_value * 0.2)
        {
            keyoff_out_time = a;
            break;
        }
    }

    if(keyoff_out_time == 0 && amplitudecurve_on.back() < peak_amplitude_value * 0.2)
        keyoff_out_time = quarter_amplitude_time;

    DurationInfo result;
    result.peak_amplitude_time = peak_amplitude_time;
    result.peak_amplitude_value = peak_amplitude_value;
    result.begin_amplitude = begin_amplitude;
    result.quarter_amplitude_time = (double)quarter_amplitude_time;
    result.keyoff_out_time = (double)keyoff_out_time;

    result.ms_sound_kon  = (int64_t)(quarter_amplitude_time * 1000.0 / interval);
    result.ms_sound_koff = (int64_t)(keyoff_out_time        * 1000.0 / interval);
    result.nosound = (peak_amplitude_value < 0.5) || ((sound_min >= -1) && (sound_max <= 1));

    fprintf(stderr, "Attack time (ms) %ld real, %f estimate\n",
        result.ms_sound_kon, solver_attack_time * 1000);
    fprintf(stderr, "Release time (ms) %ld real, %f estimate %f estimate2\n",
        result.ms_sound_koff, solver_release_time * 1000, solver_release2_time * 1000);

    in.ms_sound_kon = (uint16_t)result.ms_sound_kon;
    in.ms_sound_koff = (uint16_t)result.ms_sound_koff;
    in.is_blank = result.nosound;
}

static void MeasureDurationsDefault(FmBank::Instrument *in_p)
{
    NukedOPL3 chip;
    MeasureDurations(in_p, &chip);
}

static void MeasureDurationsBenchmark(FmBank::Instrument *in_p, OPLChipBase *chip, QVector<Measurer::BenchmarkResult> *result)
{
    std::chrono::steady_clock::time_point start, stop;
    Measurer::BenchmarkResult res;
    start = std::chrono::steady_clock::now();
    MeasureDurations(in_p, chip);
    stop  = std::chrono::steady_clock::now();
    res.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
    res.name = QString::fromUtf8(chip->emulatorName());
    result->push_back(res);
}

static void MeasureDurationsBenchmarkRunner(FmBank::Instrument *in_p, QVector<Measurer::BenchmarkResult> *result)
{
    std::vector<std::shared_ptr<OPLChipBase > > emuls =
    {
        std::shared_ptr<OPLChipBase>(new NukedOPL3v174),
        std::shared_ptr<OPLChipBase>(new NukedOPL3),
        std::shared_ptr<OPLChipBase>(new DosBoxOPL3)
    };
    for(std::shared_ptr<OPLChipBase> &p : emuls)
        MeasureDurationsBenchmark(in_p, p.get(), result);
}

Measurer::Measurer(QWidget *parent) :
    QObject(parent),
    m_parentWindow(parent)
{}

Measurer::~Measurer()
{}

static void insertOrBlank(FmBank::Instrument &ins, const FmBank::Instrument &blank, QQueue<FmBank::Instrument *> &tasks)
{
    ins.is_blank = false;
    if(memcmp(&ins, &blank, sizeof(FmBank::Instrument)) != 0)
        tasks.enqueue(&ins);
    else
    {
        ins.is_blank = true;
        ins.ms_sound_kon = 0;
        ins.ms_sound_koff = 0;
    }
}

bool Measurer::doMeasurement(FmBank &bank, FmBank &bankBackup, bool forceReset)
{
    QQueue<FmBank::Instrument *> tasks;
    FmBank::Instrument blank = FmBank::emptyInst();

    int i = 0;
    for(i = 0; i < bank.Ins_Melodic_box.size() && i < bankBackup.Ins_Melodic_box.size(); i++)
    {
        FmBank::Instrument &ins1 = bank.Ins_Melodic_box[i];
        FmBank::Instrument &ins2 = bankBackup.Ins_Melodic_box[i];
        if(forceReset || (ins1.ms_sound_kon == 0) || (memcmp(&ins1, &ins2, sizeof(FmBank::Instrument)) != 0))
            insertOrBlank(ins1, blank, tasks);
    }
    for(; i < bank.Ins_Melodic_box.size(); i++)
        insertOrBlank(bank.Ins_Melodic_box[i], blank, tasks);

    for(i = 0; i < bank.Ins_Percussion_box.size() && i < bankBackup.Ins_Percussion_box.size(); i++)
    {
        FmBank::Instrument &ins1 = bank.Ins_Percussion_box[i];
        FmBank::Instrument &ins2 = bankBackup.Ins_Percussion_box[i];
        if(forceReset || (ins1.ms_sound_kon == 0) || (memcmp(&ins1, &ins2, sizeof(FmBank::Instrument)) != 0))
            insertOrBlank(ins1, blank, tasks);
    }
    for(; i < bank.Ins_Percussion_box.size(); i++)
        insertOrBlank(bank.Ins_Percussion_box[i], blank, tasks);

    if(tasks.isEmpty())
        return true;// Nothing to do! :)

    QProgressDialog m_progressBox(m_parentWindow);
    m_progressBox.setWindowModality(Qt::WindowModal);
    m_progressBox.setWindowTitle(tr("Sounding delay calculation"));
    m_progressBox.setLabelText(tr("Please wait..."));

    #ifndef IS_QT_4
    QFutureWatcher<void> watcher;
    watcher.connect(&m_progressBox, SIGNAL(canceled()), &watcher, SLOT(cancel()));
    watcher.connect(&watcher, SIGNAL(progressRangeChanged(int,int)), &m_progressBox, SLOT(setRange(int,int)));
    watcher.connect(&watcher, SIGNAL(progressValueChanged(int)), &m_progressBox, SLOT(setValue(int)));

    watcher.setFuture(QtConcurrent::map(tasks, &MeasureDurationsDefault));

    m_progressBox.exec();
    watcher.waitForFinished();

    tasks.clear();

    // Apply all calculated values into backup store to don't re-calculate same stuff
    for(i = 0; i < bank.Ins_Melodic_box.size() && i < bankBackup.Ins_Melodic_box.size(); i++)
    {
        FmBank::Instrument &ins1 = bank.Ins_Melodic_box[i];
        FmBank::Instrument &ins2 = bankBackup.Ins_Melodic_box[i];
        ins2.ms_sound_kon  = ins1.ms_sound_kon;
        ins2.ms_sound_koff = ins1.ms_sound_koff;
        ins2.is_blank = ins1.is_blank;
    }
    for(i = 0; i < bank.Ins_Percussion_box.size() && i < bankBackup.Ins_Percussion_box.size(); i++)
    {
        FmBank::Instrument &ins1 = bank.Ins_Percussion_box[i];
        FmBank::Instrument &ins2 = bankBackup.Ins_Percussion_box[i];
        ins2.ms_sound_kon  = ins1.ms_sound_kon;
        ins2.ms_sound_koff = ins1.ms_sound_koff;
        ins2.is_blank = ins1.is_blank;
    }

    return !watcher.isCanceled();

    #else
    m_progressBox.setMaximum(tasks.size());
    m_progressBox.setValue(0);
    int count = 0;
    foreach(FmBank::Instrument *ins, tasks)
    {
        MeasureDurationsDefault(ins);
        m_progressBox.setValue(++count);
        if(m_progressBox.wasCanceled())
            return false;
    }
    return true;
    #endif
}

bool Measurer::doMeasurement(FmBank::Instrument &instrument)
{
    QProgressDialog m_progressBox(m_parentWindow);
    m_progressBox.setWindowModality(Qt::WindowModal);
    m_progressBox.setWindowTitle(tr("Sounding delay calculation"));
    m_progressBox.setLabelText(tr("Please wait..."));

#ifndef IS_QT_4
    QFutureWatcher<void> watcher;
    watcher.connect(&m_progressBox, SIGNAL(canceled()), &watcher, SLOT(cancel()));
    watcher.connect(&watcher, SIGNAL(progressRangeChanged(int,int)), &m_progressBox, SLOT(setRange(int,int)));
    watcher.connect(&watcher, SIGNAL(progressValueChanged(int)), &m_progressBox, SLOT(setValue(int)));

    watcher.setFuture(QtConcurrent::run(&MeasureDurationsDefault, &instrument));

    m_progressBox.exec();
    watcher.waitForFinished();

    return !watcher.isCanceled();

#else
    m_progressBox.show();
    MeasureDurationsDefault(&instrument);
    return true;
#endif
}

bool Measurer::runBenchmark(FmBank::Instrument &instrument, QVector<Measurer::BenchmarkResult> &result)
{
    QProgressDialog m_progressBox(m_parentWindow);
    m_progressBox.setWindowModality(Qt::WindowModal);
    m_progressBox.setWindowTitle(tr("Benchmarking emulators"));
    m_progressBox.setLabelText(tr("Please wait..."));
    m_progressBox.setCancelButton(nullptr);


#ifndef IS_QT_4
    QFutureWatcher<void> watcher;
    watcher.connect(&m_progressBox, SIGNAL(canceled()), &watcher, SLOT(cancel()));
    watcher.connect(&watcher, SIGNAL(progressRangeChanged(int,int)), &m_progressBox, SLOT(setRange(int,int)));
    watcher.connect(&watcher, SIGNAL(progressValueChanged(int)), &m_progressBox, SLOT(setValue(int)));
    watcher.connect(&watcher, SIGNAL(finished()), &m_progressBox, SLOT(accept()));

    watcher.setFuture(QtConcurrent::run(MeasureDurationsBenchmarkRunner, &instrument, &result));
    m_progressBox.exec();
    watcher.waitForFinished();
#else
    m_progressBox.show();
    MeasureDurationsBenchmarkRunner(&instrument, &result);
#endif

    return true;
}
