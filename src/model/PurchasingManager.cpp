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

#include "model/PurchasingManager.h"
#include <algorithm>
#include <iomanip>
#include "model/BusinessConnection.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/SalesManagerPrices.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "run.h"

namespace acclimate {

PurchasingManager::PurchasingManager(Storage* storage_p) : storage(storage_p) {}

void PurchasingManager::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
}

bool PurchasingManager::remove_business_connection(const BusinessConnection* business_connection) {
    auto it = std::find_if(business_connections.begin(), business_connections.end(),
                           [business_connection](const std::shared_ptr<BusinessConnection>& it) {
                               return it.get() == business_connection;
                           });
    if (it == std::end(business_connections)) {
        error("Business connection " << business_connection->id() << " not found");
    }
    business_connections.erase(it);
    if (business_connections.empty()) {
        storage->economic_agent->remove_storage(storage);
        return true;
    }
    return false;
}

const FlowQuantity PurchasingManager::get_flow_deficit() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    FlowQuantity res = FlowQuantity(0.0);
    for (const auto& bc : business_connections) {
        res += bc->get_flow_deficit();
    }
    return round(res);
}

const Flow PurchasingManager::get_transport_flow() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (const auto& bc : business_connections) {
        res += bc->get_transport_flow();
    }
    return res;
}

const Flow PurchasingManager::get_disequilibrium() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (const auto& bc : business_connections) {
        res.add_possibly_negative(bc->get_disequilibrium());
    }
    return res;
}

FloatType PurchasingManager::get_stddeviation() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    FloatType res = 0.0;
    for (const auto& bc : business_connections) {
        res += bc->get_stddeviation();
    }
    return res;
}

const Flow PurchasingManager::get_sum_of_last_shipments() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (const auto& bc : business_connections) {
        res += bc->last_shipment_Z();
    }
    return res;
}

void PurchasingManager::add_initial_demand_D_star(const Demand& demand_D_p) {
    assertstep(INITIALIZATION);
    demand_D_ += demand_D_p;
}

void PurchasingManager::subtract_initial_demand_D_star(const Demand& demand_D_p) {
    assertstep(INITIALIZATION);
    demand_D_ -= demand_D_p;
}

PurchasingManager::~PurchasingManager() {
    for (auto& business_connection : business_connections) {
        business_connection->invalidate_buyer();
    }
}

const Demand& PurchasingManager::demand_D(const EconomicAgent* const caller) const {
#ifdef DEBUG
    if (caller != storage->economic_agent) {
        assertstepnot(PURCHASE);
    }
#else
    UNUSED(caller);
#endif
    return demand_D_;
}

const Demand& PurchasingManager::initial_demand_D_star() const {
    return storage->initial_input_flow_I_star();
}

Model* PurchasingManager::model() const {
    return storage->model();
}

std::string PurchasingManager::id() const {
    return storage->sector->id() + "->" + storage->economic_agent->id();
}

#ifdef DEBUG

void PurchasingManager::print_details() const {
    info(business_connections.size() << " inputs:  I_star= " << storage->initial_input_flow_I_star().get_quantity());
    for (const auto& bc : business_connections) {
        info("    " << bc->id() << ":  Z_star= " << std::setw(11) << bc->initial_flow_Z_star().get_quantity()
                    << "  X_star= " << std::setw(11)
                    << bc->seller->firm->initial_production_X_star().get_quantity());
    }
}

#endif

}  // namespace acclimate
