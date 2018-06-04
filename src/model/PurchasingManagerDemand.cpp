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

#include "model/PurchasingManagerDemand.h"
#include "model/BusinessConnection.h"
#include "model/Model.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
PurchasingManagerDemand<ModelVariant>::PurchasingManagerDemand(Storage<ModelVariant>* storage_p) : PurchasingManager<ModelVariant>(storage_p) {}

template<class ModelVariant>
void PurchasingManagerDemand<ModelVariant>::iterate_purchase() {
    assertstep(PURCHASE);
    calc_demand_D();
    send_demand_requests_D();
}

template<class ModelVariant>
void PurchasingManagerDemand<ModelVariant>::calc_demand_D() {
    assertstep(PURCHASE);
    demand_D_ = round((std::max(FlowQuantity(0.0), storage->desired_used_flow_U_tilde().get_quantity()
                                                       + ((storage->initial_content_S_star() - storage->content_S()).get_quantity()
                                                          + get_flow_deficit() * storage->sector->model->delta_t())
                                                             / storage->sector->parameters().storage_refill_enforcement_gamma)));
}

template<class ModelVariant>
void PurchasingManagerDemand<ModelVariant>::send_demand_requests_D() {
    if (demand_D_.get_quantity() <= 0.0) {
        for (auto it = business_connections.begin(); it != business_connections.end(); ++it) {
            (*it)->calc_demand_fulfill_history();
            (*it)->send_demand_request_D(Demand(0.0));
        }
    } else {
        Demand denominator = Demand(0.0);
        for (auto it = business_connections.begin(); it != business_connections.end(); ++it) {
            (*it)->calc_demand_fulfill_history();
            denominator += (*it)->initial_flow_Z_star() * (*it)->demand_fulfill_history();
        }

        if (denominator.get_quantity() > 0.0) {
            for (auto it = business_connections.begin(); it != business_connections.end(); ++it) {
                Ratio eta = ((*it)->initial_flow_Z_star() * ((*it)->demand_fulfill_history()) / denominator);
                (*it)->send_demand_request_D(round(demand_D_ * eta));
            }
        } else {
            for (auto it = business_connections.begin(); it != business_connections.end(); ++it) {
                (*it)->send_demand_request_D(Demand(0.0));
            }
        }
    }
}

#ifdef VARIANT_DEMAND
template class PurchasingManagerDemand<VariantDemand>;
#endif
}  // namespace acclimate
