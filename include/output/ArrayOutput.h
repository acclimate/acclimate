/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
                          Christian Otto <christian.otto@pik-potsdam.de>

  This file is part of Acclimate.

  Acclimate is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  Acclimate is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with Acclimate.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ACCLIMATE_ARRAYOUTPUT_H
#define ACCLIMATE_ARRAYOUTPUT_H

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "ModelRun.h"  // for EventType
#include "acclimate.h"
#include "openmp.h"
#include "output/Output.h"

namespace settings {
class SettingsNode;
}  // namespace settings

namespace acclimate {

class EconomicAgent;
class Model;
class Sector;

class ArrayOutput : public Output {
  public:
    using output_float_t = double;

    struct Variable {
        std::string name;
        hash_t name_hash;  // does not include _quantity/_value prefix
        std::vector<output_float_t> data;

        Variable(std::string name_p, hash_t name_hash_p) : name(std::move(name_p)), name_hash(name_hash_p) {}
    };

    template<std::size_t dim>
    struct Observable {
        std::array<std::vector<std::size_t>, dim> indices;
        std::array<std::size_t, dim> sizes;
        std::vector<Variable> variables;
    };

    struct Event {
        std::size_t time = 0;
        unsigned char type = 0;
        long index1 = -1;
        long index2 = -1;
        output_float_t value = 0;
    };

  protected:
    Observable<0> obs_model;
    Observable<1> obs_firms;
    Observable<1> obs_consumers;
    Observable<1> obs_sectors;
    Observable<1> obs_regions;
    Observable<2> obs_flows;
    Observable<2> obs_storages;
    std::vector<Event> events;
    bool include_events;
    bool only_current_timestep;
    openmp::Lock event_lock;

    template<std::size_t dim>
    void resize_data(Observable<dim>& obs);

  public:
    ArrayOutput(Model* model_p, const settings::SettingsNode& settings, bool only_current_timestep_p);
    virtual ~ArrayOutput() override = default;
    void event(EventType type, const Sector* sector, const EconomicAgent* economic_agent, FloatType value) override;
    void event(EventType type, const EconomicAgent* economic_agent, FloatType value) override;
    void event(EventType type, const EconomicAgent* economic_agent_from, const EconomicAgent* economic_agent_to, FloatType value) override;
    void iterate() override;
};
}  // namespace acclimate

#endif
