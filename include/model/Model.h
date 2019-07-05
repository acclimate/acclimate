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
#include "model/GeoLocation.h"
#include "run.h"
#include "types.h"

namespace acclimate {

template<class ModelVariant>
class Consumer;
template<class ModelVariant>
class EconomicAgent;
template<class ModelVariant>
class Firm;
template<class ModelVariant>
class PurchasingManager;
template<class ModelVariant>
class Region;
template<class ModelVariant>
class Sector;

template<class ModelVariant>
class Model {
    friend class Run<ModelVariant>;

  public:
    struct Event {
        const unsigned char type;
        const Sector<ModelVariant>* sector_from;
        const Region<ModelVariant>* region_from;
        const Sector<ModelVariant>* sector_to;
        const Region<ModelVariant>* region_to;
        FloatType value;
    };
    std::vector<std::unique_ptr<Sector<ModelVariant>>> sectors;
    std::vector<std::unique_ptr<Region<ModelVariant>>> regions;
    std::vector<std::unique_ptr<GeoLocation<ModelVariant>>> other_locations;
    Sector<ModelVariant>* const consumption_sector;

  private:
    Time time_ = Time(0.0);
    TimeStep timestep_ = 0;
    unsigned char current_register_ = 1;
    Time delta_t_ = Time(1.0);
    typename ModelVariant::ModelParameters parameters_;
    bool no_self_supply_ = false;
    std::vector<std::pair<PurchasingManager<ModelVariant>*, std::size_t>> purchasing_managers;
    std::vector<std::pair<EconomicAgent<ModelVariant>*, std::size_t>> economic_agents;
    Run<ModelVariant>* const run_m;
    inline Model<ModelVariant>* model() { return this; }

  protected:
    explicit Model(Run<ModelVariant>* run_p);
    Time start_time_ = Time(0.0);
    Time stop_time_ = Time(0.0);

  public:
    inline const Time& time() const { return time_; }
    inline const TimeStep& timestep() const { return timestep_; }
    inline const Time& start_time() const { return start_time_; };
    inline const Time& stop_time() const { return stop_time_; };
    bool done() const { return time() > stop_time(); };
    inline void switch_registers() {
        assertstep(SCENARIO);
        current_register_ = 1 - current_register_;
    }
    inline void tick() {
        assertstep(SCENARIO);
        time_ += delta_t_;
        ++timestep_;
    }
    inline const Time& delta_t() const { return delta_t_; }
    inline void delta_t(const Time& delta_t_p) {
        assertstep(INITIALIZATION);
        delta_t_ = delta_t_p;
    }
    inline const bool& no_self_supply() const { return no_self_supply_; }
    inline void start_time(const Time& start_time) {
        assertstep(INITIALIZATION);
        start_time_ = start_time;
    }
    inline void stop_time(const Time& stop_time) {
        assertstep(INITIALIZATION);
        stop_time_ = stop_time;
    }
    inline void no_self_supply(bool no_self_supply_p) {
        assertstep(INITIALIZATION);
        no_self_supply_ = no_self_supply_p;
    }
    inline const unsigned char& current_register() const { return current_register_; }
    inline unsigned char other_register() const { return 1 - current_register_; }
    inline const typename ModelVariant::ModelParameters& parameters() const { return parameters_; }
    inline typename ModelVariant::ModelParameters& parameters_writable() {
        assertstep(INITIALIZATION);
        return parameters_;
    }

    Region<ModelVariant>* add_region(std::string name);
    Sector<ModelVariant>* add_sector(std::string name,
                                     const Ratio& upper_storage_limit_omega_p,
                                     const Time& initial_storage_fill_factor_psi_p,
                                     typename Sector<ModelVariant>::TransportType transport_type_p);
    void start();
    void iterate_consumption_and_production();
    void iterate_expectation();
    void iterate_purchase();
    void iterate_investment();
    Region<ModelVariant>* find_region(const std::string& name) const;
    Sector<ModelVariant>* find_sector(const std::string& name) const;
    Firm<ModelVariant>* find_firm(const std::string& sector_name, const std::string& region_name) const;
    Firm<ModelVariant>* find_firm(Sector<ModelVariant>* sector, const std::string& region_name) const;
    Consumer<ModelVariant>* find_consumer(Region<ModelVariant>* region) const;
    Consumer<ModelVariant>* find_consumer(const std::string& region_name) const;
    GeoLocation<ModelVariant>* find_location(const std::string& name) const;
    inline Run<ModelVariant>* run() const { return run_m; }
    inline std::string id() const { return "MODEL"; }
};
}  // namespace acclimate

#endif
