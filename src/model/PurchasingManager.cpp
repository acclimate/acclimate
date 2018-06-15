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
#include "model/BusinessConnection.h"
#include "model/EconomicAgent.h"
#include "model/Model.h"
#include "model/SalesManager.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
PurchasingManager<ModelVariant>::PurchasingManager(Storage<ModelVariant>* storage_p) : storage(storage_p) {}

template<class ModelVariant>
void PurchasingManager<ModelVariant>::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
}

template<class ModelVariant>
bool PurchasingManager<ModelVariant>::remove_business_connection(const BusinessConnection<ModelVariant>* business_connection) {
    for (auto it = business_connections.begin(); it != business_connections.end(); it++) {
        if (*it == business_connection) {
            business_connections.erase(it);
            break;
        }
    }
    if (business_connections.empty()) {
        storage->economic_agent->remove_storage(storage);
        return true;
    }
    return false;
}

template<class ModelVariant>
const FlowQuantity PurchasingManager<ModelVariant>::get_flow_deficit() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    FlowQuantity res = FlowQuantity(0.0);
    for (const auto& bc : business_connections) {
        res += bc->get_flow_deficit();
    }
    return round(res);
}

template<class ModelVariant>
const Flow PurchasingManager<ModelVariant>::get_transport_flow() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (const auto& bc : business_connections) {
        res += bc->get_transport_flow();
    }
    return res;
}

template<class ModelVariant>
const Flow PurchasingManager<ModelVariant>::get_disequilibrium() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (const auto& bc : business_connections) {
        res.add_possibly_negative(bc->get_disequilibrium());
    }
    return res;
}

template<class ModelVariant>
FloatType PurchasingManager<ModelVariant>::get_stddeviation() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    FloatType res = 0.0;
    for (const auto& bc : business_connections) {
        res += bc->get_stddeviation();
    }
    return res;
}

template<class ModelVariant>
const Flow PurchasingManager<ModelVariant>::get_sum_of_last_shipments() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (const auto& bc : business_connections) {
        res += bc->last_shipment_Z();
    }
    return res;
}

template<class ModelVariant>
void PurchasingManager<ModelVariant>::add_initial_demand_D_star(const Demand& demand_D_p) {
    assertstep(INITIALIZATION);
    demand_D_ += demand_D_p;
}

template<class ModelVariant>
void PurchasingManager<ModelVariant>::subtract_initial_demand_D_star(const Demand& demand_D_p) {
    assertstep(INITIALIZATION);
    demand_D_ -= demand_D_p;
}

#ifdef DEBUG
template<class ModelVariant>
void PurchasingManager<ModelVariant>::print_details() const {
    info(business_connections.size() << " inputs:  I_star= " << storage->initial_input_flow_I_star().get_quantity());
    for (const auto& bc : business_connections) {
        info("    " << std::string(*bc) << ":  Z_star= " << std::setw(11) << bc->initial_flow_Z_star().get_quantity() << "  X_star= " << std::setw(11)
                    << bc->seller->firm->initial_production_X_star().get_quantity());
    }
}
#endif

INSTANTIATE_BASIC(PurchasingManager);
}  // namespace acclimate
