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

#include "statistic.h"
#include "adldata.h"
#include <algorithm>
#include <random>
#include <string.h>

namespace Ai {

const ParameterDescription parameter_description[] =
{
#define Access(field)                                                   \
    [](const Instrument &i) -> int { return i.field; },         \
    [](Instrument &i, int x) { i.field = x; }

    /* algorithm will be a fixed value */

    {"con1", Access(connection1), PVK_Discrete, 0, 1},
    {"con2", Access(connection2), PVK_Discrete, 0, 1},
    {"fb1", Access(feedback1), PVK_Linear, 0, 7},
    {"fb2", Access(feedback2), PVK_Linear, 0, 7},

#define Operator(opnum, opnum_str)                                      \
    {"op" opnum_str "atk", Access(OP[opnum].attack), PVK_Linear, 0, 15}, \
    {"op" opnum_str "dcy", Access(OP[opnum].decay), PVK_Linear, 0, 15}, \
    {"op" opnum_str "sus", Access(OP[opnum].sustain), PVK_Linear, 0, 15}, \
    {"op" opnum_str "rel", Access(OP[opnum].release), PVK_Linear, 0, 15}, \
    {"op" opnum_str "lvl", Access(OP[opnum].level), PVK_Linear, 0, 63}, \
    {"op" opnum_str "ksl", Access(OP[opnum].ksl), PVK_Linear, 0, 3},    \
    {"op" opnum_str "fml", Access(OP[opnum].fmult), PVK_Linear, 0, 15}, \
    {"op" opnum_str "vib", Access(OP[opnum].vib), PVK_Discrete, 0, 1},  \
    {"op" opnum_str "am", Access(OP[opnum].am), PVK_Discrete, 0, 1},    \
    {"op" opnum_str "eg", Access(OP[opnum].eg), PVK_Discrete, 0, 1},    \
    {"op" opnum_str "ksr", Access(OP[opnum].ksr), PVK_Discrete, 0, 1},  \
    {"op" opnum_str "wave", Access(OP[opnum].waveform), PVK_Discrete, 0, 7}

    Operator(0, "1"),
    Operator(1, "2"),
    Operator(2, "3"),
    Operator(3, "4"),

#undef Access
#undef Operator
};

const unsigned parameter_description_count =
    sizeof(parameter_description) / sizeof(parameter_description[0]);

bool ParameterDescription::is_of_any_operator() const
{
    return name.size() > 3 && name[0] == 'o' && name[1] == 'p' &&
        name[2] >= '1' && name[2] <= '4';
}

bool ParameterDescription::is_of_nth_operator(unsigned op) const
{
    return op < 4 && name.size() > 3 &&
        name[0] == 'o' && name[1] == 'p' && name[2] == (char)('1' + op);
}

static void remove_duplicates(std::vector<Instrument> &instruments)
{
    if(instruments.empty())
        return;

    std::sort(instruments.begin(), instruments.end(),
        [](const Instrument &a, const Instrument &b) -> bool
        { return memcmp(&a, &b, sizeof(Instrument)) < 0; });

    size_t count = 1;
    for(size_t i = 1, n = instruments.size(); i < n; ++i)
    {
        if(memcmp(&instruments[i], &instruments[count - 1], sizeof(Instrument)) != 0)
            instruments[count++] = instruments[i];
    }
    instruments.resize(count);
}

void InstrumentCluster::count_parameters()
{
    parameter_counts.reset(new std::vector<size_t>[parameter_description_count]);
    parameter_totals.reset(new size_t[parameter_description_count]());

    for (unsigned i = 0; i < parameter_description_count; ++i) {
        const ParameterDescription &pd = parameter_description[i];
        unsigned numvals = (unsigned)(pd.max - pd.min + 1);
        parameter_counts[i].resize(numvals);
    }

    for (const Instrument &ins : instruments) {
        for (unsigned i = 0; i < parameter_description_count; ++i) {
            const ParameterDescription &pd = parameter_description[i];
            unsigned val = (unsigned)(pd.get(ins) - pd.min);
            ++parameter_counts[i][val];
            ++parameter_totals[i];
        }
    }
}

static std::mt19937 g_numberGenerator;

int InstrumentCluster::random_parameter_value(unsigned parameter, double experimental) const
{
    const std::vector<size_t> &counts = parameter_counts[parameter];
    size_t total = parameter_totals[parameter];

    unsigned num_values = counts.size();
    if (num_values == 0)
        return 0;

    std::unique_ptr<double[]> probability(new double[num_values]);
    if(total == 0) {
        for (unsigned i = 0; i < num_values; ++i)
            probability[i] = 1.0 / num_values;
    }
    else
    {
        for (unsigned i = 0; i < num_values; ++i) {
            double p = counts[i] / (double)total;
            probability[i] = (1.0 - experimental) * p + experimental * (1.0 - p);
        }
    }

    double rand = g_numberGenerator() * (1.0 / g_numberGenerator.max());

    unsigned value_index = 0;
    while(value_index + 1 < num_values && rand > probability[value_index])
        rand -= probability[value_index++];

    return (int)value_index + parameter_description[parameter].min;
}

InstrumentClustering InstrumentClustering::by_algorithm_and_type()
{
    InstrumentClustering icls;
    std::vector<InstrumentCluster> &clusters = icls.clusters;

    clusters.clear();
    clusters.resize(
        2 /* melodic or percussive */ *
        6 /* algorithm FM/AM/FM-FM/FM-AM/AM-FM/AM-AM */);

    std::vector<Instrument> meloInstruments;
    std::vector<Instrument> drumInstruments;

    unsigned adlbankCount = ::maxAdlBanks();
    meloInstruments.reserve(128 * adlbankCount);
    drumInstruments.reserve(128 * adlbankCount);

    for(unsigned b = 0; b < adlbankCount; ++b)
    {
        for(unsigned i = 0; i < 256; ++i)
        {
            Instrument ins;
            cvt_ADLI_to_FMIns(ins, ::adlins[::banks[b][i]]);
            if (!ins.is_blank)
                ((b < 128) ? meloInstruments : drumInstruments).push_back(ins);
        }
    }

    remove_duplicates(meloInstruments);
    remove_duplicates(drumInstruments);
    size_t meloCount = meloInstruments.size();
    size_t drumCount = drumInstruments.size();

    for(size_t i = 0, n = clusters.size(); i < n; ++i)
    {
        clusters[i].instruments.reserve((i < n / 2) ? meloCount : drumCount);
        const char *name1[] = {"Melodic", "Percussive"};
        const char *name2[] = {"FM", "AM", "FM-FM", "FM-AM", "AM-FM", "AM-AM"};
        clusters[i].name = std::string(name1[i >= n / 2]) + ' ' + std::string(name2[i % 6]);
        clusters[i].percussion = i >= n / 2;
    }

    for(size_t i = 0; i < meloCount + drumCount; ++i)
    {
        const Instrument &ins = (i < meloCount) ?
            meloInstruments[i] : drumInstruments[i - meloCount];

        if(!ins.en_4op)
        {
            InstrumentCluster &icl = clusters[icls.cluster_of((i >= meloCount), false, ins.connection1, false)];
            icl.instruments.push_back(ins);
        }
        else if(!ins.en_pseudo4op)
        {
            InstrumentCluster &icl = clusters[icls.cluster_of((i >= meloCount), true, ins.connection1, ins.connection2)];
            icl.instruments.push_back(ins);
        }
        else
        {
            InstrumentCluster &icl1 = clusters[icls.cluster_of((i >= meloCount), false, ins.connection1, false)];
            Instrument ins1 = ins;
            ins1.en_4op = false;
            ins1.en_pseudo4op = false;
            icl1.instruments.push_back(ins1);

            InstrumentCluster &icl2 = clusters[icls.cluster_of((i >= meloCount), false, ins.connection2, false)];
            Instrument ins2 = ins1;
            std::swap(ins2.OP[0], ins2.OP[2]);
            std::swap(ins2.OP[1], ins2.OP[3]);
            icl2.instruments.push_back(ins2);
        }
    }

    for (InstrumentCluster &cluster : clusters)
        cluster.count_parameters();

    return icls;
}

Instrument InstrumentClustering::generate_from_same_cluster(const Instrument &ins_, bool percussive, double experimental)
{
    Instrument ins = ins_;

    InstrumentCluster *icl1, *icl2;
    if(!ins.en_4op)
    {
        icl1 = &clusters[cluster_of(percussive, false, ins.connection1, false)];
        icl2 = icl1;
    }
    else if(!ins.en_pseudo4op)
    {
        icl1 = &clusters[cluster_of(percussive, true, ins.connection1, ins.connection2)];
        icl2 = icl1;
    }
    else
    {
        icl1 = &clusters[cluster_of(percussive, false, ins.connection1, false)];
        icl2 = &clusters[cluster_of(percussive, false, ins.connection2, false)];
    }

    for(size_t i = 0; i < parameter_description_count; ++i)
    {
        const ParameterDescription &pd = parameter_description[i];
        const InstrumentCluster &icl =
            (pd.is_of_nth_operator(2) || pd.is_of_nth_operator(3)) ?
            *icl2 : *icl1;
        pd.set(ins, icl.random_parameter_value(i, experimental));
    }

    return ins;
}

size_t InstrumentClustering::cluster_of(bool percussion, bool fourop, bool con1, bool con2) const
{
    if(fourop)
        return (percussion ? 6 : 0) + 2 + ((con1 << 1) | con2);
    else
        return (percussion ? 6 : 0) + con1;
}

}  // namespace Ai
