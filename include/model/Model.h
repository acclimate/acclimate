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

#include "model/Sector.h"
#include "parameters.h"
#include "types.h"

namespace acclimate {

class Run;
class Consumer;
class EconomicAgent;
class GeoLocation;
class Firm;
class PurchasingManager;
class Region;

class Model {
    friend class Run;

  public:
    std::vector<std::unique_ptr<Sector>> sectors;
    std::vector<std::unique_ptr<Region>> regions;
    std::vector<std::unique_ptr<GeoLocation>> other_locations;
    Sector* const consumption_sector;

  private:
    Time time_ = Time(0.0);
    TimeStep timestep_ = 0;
    unsigned char current_register_ = 1;
    Time delta_t_ = Time(1.0);
    Parameters::ModelParameters parameters_;
    bool no_self_supply_ = false;
    std::vector<std::pair<PurchasingManager*, std::size_t>> purchasing_managers;
    std::vector<std::pair<EconomicAgent*, std::size_t>> economic_agents;
    Run* const run_m;

    inline Model* model() { return this; }

  protected:
    explicit Model(Run* run_p);
    Time start_time_ = Time(0.0);
    Time stop_time_ = Time(0.0);

  public:
    inline const Time& time() const { return time_; }

    inline const TimeStep& timestep() const { return timestep_; }

    inline const Time& start_time() const { return start_time_; };

    inline const Time& stop_time() const { return stop_time_; };

    bool done() const { return time() > stop_time(); };
    void switch_registers();
    void tick();

    inline const Time& delta_t() const { return delta_t_; }

    void delta_t(const Time& delta_t_p);

    inline const bool& no_self_supply() const { return no_self_supply_; }

    void start_time(const Time& start_time);
    void stop_time(const Time& stop_time);
    void no_self_supply(bool no_self_supply_p);

    inline const unsigned char& current_register() const { return current_register_; }

    inline unsigned char other_register() const { return 1 - current_register_; }

    inline const Parameters::ModelParameters& parameters() const { return parameters_; }

    Parameters::ModelParameters& parameters_writable();

    Region* add_region(std::string name);
    Sector* add_sector(std::string name,
                       const Ratio& upper_storage_limit_omega_p,
                       const Time& initial_storage_fill_factor_psi_p,
                       typename Sector::TransportType transport_type_p);
    void start();
    void iterate_consumption_and_production();
    void iterate_expectation();
    void iterate_purchase();
    void iterate_investment();
    Region* find_region(const std::string& name) const;
    Sector* find_sector(const std::string& name) const;
    Firm* find_firm(const std::string& sector_name, const std::string& region_name) const;
    Firm* find_firm(Sector* sector, const std::string& region_name) const;
    Consumer* find_consumer(Region* region) const;
    Consumer* find_consumer(const std::string& region_name) const;
    GeoLocation* find_location(const std::string& name) const;

    inline Run* run() const { return run_m; }

    inline std::string id() const { return "MODEL"; }
};
}  // namespace acclimate

#endif
