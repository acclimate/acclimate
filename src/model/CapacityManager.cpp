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

#include "model/CapacityManager.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/Storage.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
CapacityManager<ModelVariant>::CapacityManager(Firm<ModelVariant>* firm_p, const Ratio& possible_overcapacity_ratio_beta_p)
    : firm(firm_p), possible_overcapacity_ratio_beta(possible_overcapacity_ratio_beta_p) {}

template<class ModelVariant>
const Flow CapacityManager<ModelVariant>::get_possible_production_X_hat() const {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    Ratio possible_production_capacity_p_hat = firm->forcing() * possible_overcapacity_ratio_beta;

    for (const auto& is : firm->input_storages) {
        Ratio tmp = is->get_possible_use_U_hat() / is->initial_used_flow_U_star();
        if (tmp < possible_production_capacity_p_hat) {
            possible_production_capacity_p_hat = tmp;
        }
    }

    assert(possible_production_capacity_p_hat >= 0.0);

    return round(firm->initial_production_X_star() * possible_production_capacity_p_hat);
}

template<class ModelVariant>
Ratio CapacityManager<ModelVariant>::get_production_capacity_p() const {
    return firm->production_X() / firm->initial_production_X_star();
}

template<class ModelVariant>
Ratio CapacityManager<ModelVariant>::get_desired_production_capacity_p_tilde() const {
    return desired_production_X_tilde_ / firm->initial_production_X_star();
}

template<class ModelVariant>
Ratio CapacityManager<ModelVariant>::get_possible_production_capacity_p_hat() const {
    return possible_production_X_hat_ / firm->initial_production_X_star();
}

template<class ModelVariant>
void CapacityManager<ModelVariant>::calc_possible_and_desired_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    possible_production_X_hat_ = get_possible_production_X_hat();
    desired_production_X_tilde_ = firm->sales_manager->sum_demand_requests_D();
}

template<class ModelVariant>
const Flow CapacityManager<ModelVariant>::calc_production_X() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    calc_possible_and_desired_production();
    return std::min(possible_production_X_hat_, desired_production_X_tilde_);
}

INSTANTIATE_BASIC(CapacityManager);
}  // namespace acclimate
