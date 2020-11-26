/*
  Copyright (C) 2014-2017 Sven Willner <sven.willner@pik-potsdam.de>
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

#ifndef ACCLIMATE_MODEL_H
#define ACCLIMATE_MODEL_H

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "acclimate.h"
#include "model/GeoLocation.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "parameters.h"

namespace acclimate {

class Consumer;
class EconomicAgent;
class Firm;
class ModelRun;
class PurchasingManager;

class Model {
    friend class ModelRun;

  private:
    Time time_ = Time(0.0);
    Time start_time_ = Time(0.0);
    Time stop_time_ = Time(0.0);
    TimeStep timestep_ = 0;
    Time delta_t_ = Time(1.0);
    unsigned char current_register_ = 1;
    Parameters::ModelParameters parameters_;
    bool no_self_supply_ = false;
    std::vector<std::pair<PurchasingManager*, std::size_t>> purchasing_managers;
    non_owning_ptr<ModelRun> run_m;

  public:
    owning_vector<Sector> sectors;
    owning_vector<Region> regions;
    owning_vector<GeoLocation> other_locations;
    owning_vector<EconomicAgent> economic_agents;

  private:
    explicit Model(ModelRun* run_p);

  public:
    Model(const Model& other) = delete;
    Model(Model&& other) = default;
    ~Model();
    const Time& time() const { return time_; }
    const Time& start_time() const { return start_time_; };
    const Time& stop_time() const { return stop_time_; };
    const TimeStep& timestep() const { return timestep_; }
    const Time& delta_t() const { return delta_t_; }
    bool done() const { return time() > stop_time(); };
    bool is_first_timestep() const { return timestep_ == 0; }
    void switch_registers();
    void tick();
    const bool& no_self_supply() const { return no_self_supply_; }
    void set_start_time(const Time& start_time);
    void set_stop_time(const Time& stop_time);
    void set_delta_t(const Time& delta_t_p);
    void no_self_supply(bool no_self_supply_p);
    const unsigned char& current_register() const { return current_register_; }
    unsigned char other_register() const { return 1 - current_register_; }
    const Parameters::ModelParameters& parameters() const { return parameters_; }
    Parameters::ModelParameters& parameters_writable();
    void start();
    void iterate_consumption_and_production();
    void iterate_expectation();
    void iterate_purchase();
    void iterate_investment();

    ModelRun* run() { return run_m; }
    const ModelRun* run() const { return run_m; }
    const Model* model() const { return this; }
    std::string name() const { return "MODEL"; }
};
}  // namespace acclimate

#endif
