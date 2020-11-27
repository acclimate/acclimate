/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
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

#include "model/Consumer.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "ModelRun.h"
#include "acclimate.h"
#include "model/Model.h"
#include "model/PurchasingManager.h"
#include "model/Region.h"
#include "model/Storage.h"
#include "parameters.h"

namespace acclimate {

Consumer::Consumer(Region* region_p) : EconomicAgent(region_p->model()->consumption_sector, region_p, EconomicAgent::Type::CONSUMER) {}

void Consumer::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    for (const auto& is : input_storages) {
        Flow possible_used_flow_U_hat = is->get_possible_use_U_hat();  // Price(U_hat) = Price of used flow
        Price reservation_price(0.0);
        if (possible_used_flow_U_hat.get_quantity() > 0.0) {
            // we have to purchase with the average price of input and storage
            reservation_price = possible_used_flow_U_hat.get_price();
        } else {  // possible used flow is zero
            Price last_reservation_price = is->desired_used_flow_U_tilde(this).get_price();
            assert(!isnan(last_reservation_price));
            // price is calculated from last desired used flow
            reservation_price = last_reservation_price;
            model()->run()->event(EventType::NO_CONSUMPTION, this, nullptr, to_float(last_reservation_price));
        }
        assert(reservation_price > 0.0);

        const Flow desired_used_flow_U_tilde = Flow(round(is->baseline_used_flow_U_star().get_quantity() * forcing_
                                                          * pow(reservation_price / Price(1.0), is->parameters().consumption_price_elasticity)),
                                                    reservation_price);
        const Flow used_flow_U = Flow(std::min(desired_used_flow_U_tilde.get_quantity(), possible_used_flow_U_hat.get_quantity()), reservation_price);
        is->set_desired_used_flow_U_tilde(desired_used_flow_U_tilde);
        is->use_content_S(round(used_flow_U));
        region->add_consumption_flow_Y(round(used_flow_U));
        is->iterate_consumption_and_production();
    }
}

void Consumer::iterate_expectation() { debug::assertstep(this, IterationStep::EXPECTATION); }

void Consumer::iterate_purchase() {
    debug::assertstep(this, IterationStep::PURCHASE);
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_purchase();
    }
}

void Consumer::iterate_investment() {
    debug::assertstep(this, IterationStep::INVESTMENT);
//    growth_rate_ = 5.425525e-5;
//    for (const auto& is : input_storages) {
//        is->iterate_investment();
//    }
}

void Consumer::print_details() const {
    if constexpr (options::DEBUGGING) {
        log::info(this);
        for (const auto& is : input_storages) {
            is->purchasing_manager->print_details();
        }
    }
}

}  // namespace acclimate
