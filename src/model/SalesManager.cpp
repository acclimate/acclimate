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
#include <algorithm>
#include <iomanip>
#include "model/BusinessConnection.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "run.h"

namespace acclimate {

SalesManager::SalesManager(Firm* firm_p) : firm(firm_p) {}

void SalesManager::add_demand_request_D(const Demand& demand_request_D) {
    assertstep(PURCHASE);
    firm->sector->add_demand_request_D(demand_request_D);
    sum_demand_requests_D_lock.call([&]() { sum_demand_requests_D_ += demand_request_D; });
}

void SalesManager::add_initial_demand_request_D_star(const Demand& initial_demand_request_D_star) {
    assertstep(INITIALIZATION);
    sum_demand_requests_D_ += initial_demand_request_D_star;
}

void SalesManager::subtract_initial_demand_request_D_star(const Demand& initial_demand_request_D_star) {
    assertstep(INITIALIZATION);
    sum_demand_requests_D_ -= initial_demand_request_D_star;
}

void SalesManager::iterate_expectation() {
    assertstep(EXPECTATION);
    sum_demand_requests_D_ = Flow(0.0);
}

const Flow SalesManager::get_transport_flow() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (const auto& bc : business_connections) {
        res += bc->get_transport_flow();
    }
    return res;
}

bool SalesManager::remove_business_connection(BusinessConnection* business_connection) {
    auto it = std::find_if(business_connections.begin(), business_connections.end(),
                           [business_connection](const std::shared_ptr<BusinessConnection>& it) {
                               return it.get() == business_connection;
                           });
    if (it == std::end(business_connections)) {
        error("Business connection " << business_connection->id() << " not found");
    }
    business_connections.erase(it);
#ifdef DEBUG
    if (business_connections.empty()) {
        assertstep(INITIALIZATION);
        return true;
    }
#endif
    return false;
}

SalesManager::~SalesManager() {
    for (auto& business_connection : business_connections) {
        business_connection->invalidate_seller();
    }
}

#ifdef DEBUG

void SalesManager::print_details() const {
    info(business_connections.size() << " outputs:");
    for (const auto& bc : business_connections) {
        info("    " << bc->id() << "  Z_star= " << std::setw(11) << bc->initial_flow_Z_star().get_quantity());
    }
}

#endif

Model* SalesManager::model() const {
    return firm->model();
}

std::string SalesManager::id() const {
    return firm->id();
}

const Demand& SalesManager::sum_demand_requests_D() const {
    assertstepnot(PURCHASE);
    return sum_demand_requests_D_;
}

}  // namespace acclimate
