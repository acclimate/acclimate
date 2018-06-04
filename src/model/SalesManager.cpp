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

#include "model/SalesManager.h"
#include "model/BusinessConnection.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/PurchasingManager.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
SalesManager<ModelVariant>::SalesManager(Firm<ModelVariant>* firm_p) : firm(firm_p) {}

template<class ModelVariant>
void SalesManager<ModelVariant>::add_demand_request_D(const Demand& demand_request_D) {
    assertstep(PURCHASE);
    firm->sector->add_demand_request_D(demand_request_D);
#pragma omp critical(sum_demand_requests_D_)
    { sum_demand_requests_D_ += demand_request_D; }
}

template<class ModelVariant>
void SalesManager<ModelVariant>::add_initial_demand_request_D_star(const Demand& initial_demand_request_D_star) {
    assertstep(INITIALIZATION);
    sum_demand_requests_D_ += initial_demand_request_D_star;
}

template<class ModelVariant>
void SalesManager<ModelVariant>::subtract_initial_demand_request_D_star(const Demand& initial_demand_request_D_star) {
    assertstep(INITIALIZATION);
    sum_demand_requests_D_ -= initial_demand_request_D_star;
}

template<class ModelVariant>
void SalesManager<ModelVariant>::iterate_expectation() {
    assertstep(EXPECTATION);
    sum_demand_requests_D_ = Flow(0.0);
}

template<class ModelVariant>
const Flow SalesManager<ModelVariant>::get_transport_flow() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (const auto& bc : business_connections) {
        res += bc->get_total_flow();
    }
    return res;
}

template<class ModelVariant>
std::unique_ptr<BusinessConnection<ModelVariant>> SalesManager<ModelVariant>::remove_business_connection(
    BusinessConnection<ModelVariant>* business_connection) {
    for (auto it = business_connections.begin(); it != business_connections.end(); it++) {
        if (it->get() == business_connection) {
            std::unique_ptr<BusinessConnection<ModelVariant>> res = std::move(*it);
            business_connections.erase(it);
            return res;
        }
    }
#ifdef DEBUG
    if (business_connections.empty()) {
        assertstep(INITIALIZATION);
    }
#endif
    std::unique_ptr<BusinessConnection<ModelVariant>> res;
    return res;
}

#ifdef DEBUG
template<class ModelVariant>
void SalesManager<ModelVariant>::print_details() const {
    info(business_connections.size() << " outputs:");
    for (const auto& bc : business_connections) {
        info("    " << bc->id() << "  Z_star= " << std::setw(11) << bc->initial_flow_Z_star().get_quantity());
    }
}
#endif

INSTANTIATE_BASIC(SalesManager);
}  // namespace acclimate
