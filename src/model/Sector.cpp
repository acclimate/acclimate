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

#include "ModelRun.h"
#include "acclimate.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "settingsnode.h"

namespace acclimate {

Sector::Sector(Model* model_p,
               std::string id_p,
               IndexType index_p,
               Ratio upper_storage_limit_omega_p,
               Time initial_storage_fill_factor_psi_p,
               TransportType transport_type_p)
    : id_m(std::move(id_p)),
      index_m(index_p),
      model_m(model_p),
      upper_storage_limit_omega(upper_storage_limit_omega_p),
      initial_storage_fill_factor_psi(std::move(initial_storage_fill_factor_psi_p)),
      transport_type(transport_type_p) {}

void Sector::add_demand_request_D(const Demand& demand_request_D) {
    debug::assertstep(this, IterationStep::PURCHASE);
    total_demand_D_lock.call([&]() { total_demand_D_ += demand_request_D; });
}

void Sector::add_production_X(const Flow& production_X) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    total_production_X_lock.call([&]() { total_production_X_m += production_X; });
}

void Sector::add_initial_production_X(const Flow& production_X) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    last_total_production_X_m += production_X;
    total_production_X_m += production_X;
}

void Sector::subtract_initial_production_X(const Flow& production_X) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    last_total_production_X_m -= production_X;
    total_production_X_m -= production_X;
}

void Sector::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    total_demand_D_ = Demand(0.0);
    last_total_production_X_m = total_production_X_m;
    total_production_X_m = Flow(0.0);
}

void Sector::remove_firm(Firm* firm) {
    for (auto it = firms.begin(); it != firms.end(); ++it) {  // TODO use find_if
        if (*it == firm) {
            firms.erase(it);
            break;
        }
    }
}

Sector::TransportType Sector::map_transport_type(const settings::hstring& transport_type) {
    switch (transport_type) {
        case settings::hstring::hash("aviation"):
            return TransportType::AVIATION;
        case settings::hstring::hash("immediate"):
            return TransportType::IMMEDIATE;
        case settings::hstring::hash("roadsea"):
            return TransportType::ROADSEA;
        default:
            throw log::error("Unknown transport type ", transport_type);
    }
}

const char* Sector::unmap_transport_type(Sector::TransportType transport_type) {
    switch (transport_type) {
        case TransportType::AVIATION:
            return "aviation";
        case TransportType::IMMEDIATE:
            return "immediate";
        case TransportType::ROADSEA:
            return "roadsea";
        default:
            throw log::error("Unkown transport type");
    }
}

const Demand& Sector::total_demand_D() const {
    debug::assertstepnot(this, IterationStep::PURCHASE);
    return total_demand_D_;
}

const Demand& Sector::total_production_X() const {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return total_production_X_m;
}

Parameters::SectorParameters& Sector::parameters_writable() {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    return parameters_m;
}

}  // namespace acclimate
