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

#ifndef ACCLIMATE_SECTOR_H
#define ACCLIMATE_SECTOR_H

#include <string>

#include "acclimate.h"
#include "openmp.h"
#include "parameters.h"

namespace acclimate {

class Model;
class Firm;

class Sector final {
  public:
    enum class transport_type_t { AVIATION, IMMEDIATE, ROADSEA };

  private:
    Demand total_demand_D_ = Demand(0.0);
    openmp::Lock total_demand_D_lock;
    Flow total_production_X_m = Flow(0.0);
    openmp::Lock total_production_X_lock;
    Flow last_total_production_X_m = Flow(0.0);
    Parameters::SectorParameters parameters_m;
    non_owning_ptr<Model> model_m;

  public:
    const id_t id;
    const Ratio upper_storage_limit_omega;
    const Time initial_storage_fill_factor_psi;
    const transport_type_t transport_type;
    non_owning_vector<Firm> firms;

  public:
    Sector(Model* model_p, id_t id_p, Ratio upper_storage_limit_omega_p, Time initial_storage_fill_factor_psi_p, transport_type_t transport_type_p);
    static transport_type_t map_transport_type(const hashed_string& transport_type);
    static const char* unmap_transport_type(transport_type_t transport_type);
    const Demand& total_demand_D() const;
    const Demand& total_production_X() const;
    const Parameters::SectorParameters& parameters() const { return parameters_m; }
    Parameters::SectorParameters& parameters_writable();
    void add_demand_request_D(const Demand& demand_request_D);
    void add_production_X(const Flow& production_X);
    void add_initial_production_X(const Flow& production_X);
    void subtract_initial_production_X(const Flow& production_X);
    void iterate_consumption_and_production();

    Model* model() { return model_m; }
    const Model* model() const { return model_m; }
    const std::string& name() const { return id.name; }

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return true  //
               && o.set(H::hash("offer_price"),
                        [this]() {  //
                            return total_production_X().get_price();
                        })
               && o.set(H::hash("total_production"),
                        [this]() {  //
                            return total_production_X();
                        })
               && o.set(H::hash("total_demand"),
                        [this]() {  //
                            return total_demand_D();
                        })
            //
            ;
    }
};
}  // namespace acclimate

#endif
