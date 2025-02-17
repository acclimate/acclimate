// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

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

        Variable(std::string name_, hash_t name_hash_) : name(std::move(name_)), name_hash(name_hash_) {}
    };

    template<std::size_t dim>
    struct Observable {
        std::array<std::vector<unsigned long long>, dim> indices;
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
    Observable<0> obs_model_;
    Observable<1> obs_firms_;
    Observable<1> obs_consumers_;
    Observable<1> obs_sectors_;
    Observable<1> obs_regions_;
    Observable<1> obs_locations_;
    Observable<2> obs_flows_;
    Observable<2> obs_storages_;
    std::vector<Event> events_;
    bool include_events_;
    bool only_current_timestep_;
    openmp::Lock event_lock_;

    template<std::size_t dim>
    void resize_data(Observable<dim>& obs);

  public:
    ArrayOutput(Model* model, const settings::SettingsNode& settings, bool only_current_timestep);
    virtual ~ArrayOutput() override = default;
    void event(EventType type, const Sector* sector, const EconomicAgent* economic_agent, FloatType value) override;
    void event(EventType type, const EconomicAgent* economic_agent, FloatType value) override;
    void event(EventType type, const EconomicAgent* economic_agent_from, const EconomicAgent* economic_agent_to, FloatType value) override;
    void iterate() override;
};
}  // namespace acclimate

#endif
