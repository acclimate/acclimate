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

#include "model/CapacityManagerPrices.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/Storage.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
CapacityManagerPrices<ModelVariant>::CapacityManagerPrices(Firm<ModelVariant>* firm_p, Ratio possible_overcapacity_ratio_beta_p)
    : CapacityManager<ModelVariant>(firm_p, possible_overcapacity_ratio_beta_p) {}

#ifdef DEBUG
template<class ModelVariant>
void CapacityManagerPrices<ModelVariant>::print_inputs() const {
    info(std::string(*this) << ": " << firm->input_storages.size() << " inputs:");
    for (const auto& is : firm->input_storages) {
        Flow possible_use_U_hat = is->get_possible_use_U_hat();
        info("    " << std::string(*is) << ":"

                    << "  U_hat= " << std::setw(11) << possible_use_U_hat.get_quantity()

                    << "  n_hat= " << std::setw(11) << possible_use_U_hat.get_price()

                    << "  n_hat*a= " << std::setw(11) << (possible_use_U_hat.get_price() * is->get_technology_coefficient_a()));
    }
}
#endif

template<class ModelVariant>
const Flow CapacityManagerPrices<ModelVariant>::get_possible_production_X_hat_intern(bool consider_transport_in_production_costs) const {
    Ratio possible_production_capacity_p_hat = firm->forcing() * possible_overcapacity_ratio_beta;
    Price unit_commodity_costs = Price(0.0);

    for (auto& input_storage : firm->input_storages) {
        Flow possible_use_U_hat = input_storage->get_possible_use_U_hat();
        if (consider_transport_in_production_costs) {
            Flow total_flow = input_storage->purchasing_manager->get_total_flow();
            unit_commodity_costs += (possible_use_U_hat + total_flow).get_price() * input_storage->get_technology_coefficient_a();
        } else {
            unit_commodity_costs += possible_use_U_hat.get_price() * input_storage->get_technology_coefficient_a();
        }
        Ratio tmp = possible_use_U_hat / input_storage->initial_used_flow_U_star();
        if (tmp < possible_production_capacity_p_hat) {
            possible_production_capacity_p_hat = tmp;
        }
    }
    assert(possible_production_capacity_p_hat >= 0.0);
    Flow result = round(firm->initial_production_X_star() * possible_production_capacity_p_hat);
    // note: if result.get_quantity() == 0.0 price is NAN
    if (result.get_quantity() > 0.0) {
        result.set_price(round(unit_commodity_costs + firm->sales_manager->get_initial_unit_variable_production_costs()));
    }
    return result;
}

template<class ModelVariant>
const Flow CapacityManagerPrices<ModelVariant>::get_possible_production_X_hat() const {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    bool consider_transport_in_production_costs = false;
    return get_possible_production_X_hat_intern(consider_transport_in_production_costs);
}

template<class ModelVariant>
const Flow CapacityManagerPrices<ModelVariant>::estimate_possible_production_X_hat() const {
    assertstep(EXPECTATION);
    bool consider_transport_in_production_costs = true;
    return get_possible_production_X_hat_intern(consider_transport_in_production_costs);
}

template<class ModelVariant>
const Flow CapacityManagerPrices<ModelVariant>::calc_production_X() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    calc_possible_and_desired_production();
    return firm->sales_manager->calc_production_X();
}

INSTANTIATE_PRICES(CapacityManagerPrices);
}  // namespace acclimate
