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

#include "bank.h"
#include <functional>
#include <memory>

namespace Ai {

typedef FmBank::Instrument Instrument;

enum ParameterValueKind {
    PVK_Linear,
    PVK_Discrete
};

struct ParameterDescription {
    ParameterDescription(
        const std::string &name,
        std::function<int(const Instrument &)> get,
        std::function<void(Instrument &, int)> set,
        ParameterValueKind pvk, int min, int max)
        : name(name), get(get), set(set), pvk(pvk), min(min), max(max)
        {
        }

    bool is_of_any_operator() const;
    bool is_of_nth_operator(unsigned op) const;

    std::string name;
    std::function<int(const Instrument &)> get;
    std::function<void(Instrument &, int)> set;
    ParameterValueKind pvk = PVK_Linear;
    int min = 0;
    int max = 0;
};

extern const ParameterDescription parameter_description[];
extern const unsigned parameter_description_count;

struct InstrumentCluster
{
    std::string name;
    bool percussion = false;
    std::vector<Instrument> instruments;
    std::unique_ptr<std::vector<size_t>[]> parameter_counts;
    std::unique_ptr<size_t[]> parameter_totals;

    void count_parameters();
    int random_parameter_value(unsigned parameter, double experimental) const;
};

struct InstrumentClustering
{
    std::vector<InstrumentCluster> clusters;

    static InstrumentClustering by_algorithm_and_type();
    Instrument generate_from_same_cluster(const Instrument &ins, bool percussive, double experimental);
    size_t cluster_of(bool percussion, bool fourop, bool con1, bool con2) const;
};

}  // namespace Ai
