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

#include "model/Sector.h"
#include <utility>
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Sector<ModelVariant>::Sector(Model<ModelVariant>* model_p,
                             std::string id_p,
                             const IntType index_p,
                             const Ratio& upper_storage_limit_omega_p,
                             const Time& initial_storage_fill_factor_psi_p,
                             TransportType transport_type_p)
    : id_m(std::move(id_p)),
      index_m(index_p),
      model_m(model_p),
      upper_storage_limit_omega(upper_storage_limit_omega_p),
      initial_storage_fill_factor_psi(initial_storage_fill_factor_psi_p),
      transport_type(transport_type_p) {}

template<class ModelVariant>
void Sector<ModelVariant>::add_demand_request_D(const Demand& demand_request_D) {
    assertstep(PURCHASE);
    total_demand_D_lock.call([&]() { total_demand_D_ += demand_request_D; });
}

template<class ModelVariant>
void Sector<ModelVariant>::add_production_X(const Flow& production_X) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    total_production_X_lock.call([&]() { total_production_X_m += production_X; });
}

template<class ModelVariant>
void Sector<ModelVariant>::add_initial_production_X(const Flow& production_X) {
    assertstep(INITIALIZATION);
    last_total_production_X_m += production_X;
    total_production_X_m += production_X;
}

template<class ModelVariant>
void Sector<ModelVariant>::subtract_initial_production_X(const Flow& production_X) {
    assertstep(INITIALIZATION);
    last_total_production_X_m -= production_X;
    total_production_X_m -= production_X;
}

template<class ModelVariant>
void Sector<ModelVariant>::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    total_demand_D_ = Demand(0.0);
    last_total_production_X_m = total_production_X_m;
    total_production_X_m = Flow(0.0);
}

template<class ModelVariant>
void Sector<ModelVariant>::remove_firm(Firm<ModelVariant>* firm) {
    for (auto it = firms.begin(); it != firms.end(); ++it) { // TODO use find_if
        if (*it == firm) {
            firms.erase(it);
            break;
        }
    }
}

INSTANTIATE_BASIC(Sector);
}  // namespace acclimate
