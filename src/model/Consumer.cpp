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

#include "model/Consumer.h"
#include "model/Model.h"
#include "model/PurchasingManagerPrices.h"
#include "model/Region.h"
#include "model/Storage.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Consumer<ModelVariant>::Consumer(Region<ModelVariant>* region_p)
    : EconomicAgent<ModelVariant>(region_p->model->consumption_sector, region_p, EconomicAgent<ModelVariant>::Type::CONSUMER) {}

template<class ModelVariant>
inline Consumer<ModelVariant>* Consumer<ModelVariant>::as_consumer() {
    return this;
}

template<>
void Consumer<VariantBasic>::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    for (const auto& is : input_storages) {
        Flow desired_used_flow_U_tilde = round(is->initial_input_flow_I_star() * forcing_);
        Flow used_flow_U = round(std::min(desired_used_flow_U_tilde, is->get_possible_use_U_hat()));

        is->set_desired_used_flow_U_tilde(desired_used_flow_U_tilde);
        is->use_content_S(used_flow_U);
        region->add_consumption_flow_Y(used_flow_U);
        is->iterate_consumption_and_production();
    }
}

template<>
void Consumer<VariantDemand>::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    for (const auto& is : input_storages) {
        Flow desired_used_flow_U_tilde = round(is->initial_input_flow_I_star() * forcing_);
        Flow used_flow_U = round(std::min(desired_used_flow_U_tilde, is->get_possible_use_U_hat()));

        is->set_desired_used_flow_U_tilde(desired_used_flow_U_tilde);
        is->use_content_S(used_flow_U);
        region->add_consumption_flow_Y(used_flow_U);
        is->iterate_consumption_and_production();
    }
}

template<class ModelVariant>
void Consumer<ModelVariant>::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    for (const auto& is : input_storages) {
        Flow possible_used_flow_U_hat = is->get_possible_use_U_hat();  // Price(U_hat) = Price of used flow
        Price reservation_price = Price(0.0);
        if (possible_used_flow_U_hat.get_quantity() > 0.0) {
            // we have to purchase with the average price of input and storage
            reservation_price = possible_used_flow_U_hat.get_price();
        } else {  // possible used flow is zero
            Price last_reservation_price = is->desired_used_flow_U_tilde(this).get_price();
            assert(!isnan(last_reservation_price));
            // price is calculated from last desired used flow
            reservation_price = last_reservation_price;
            Acclimate::Run<ModelVariant>::instance()->event(EventType::NO_CONSUMPTION, this, nullptr, to_float(last_reservation_price));
        }
        assert(reservation_price > 0.0);

        const Flow desired_used_flow_U_tilde = Flow(round(is->initial_input_flow_I_star().get_quantity() * forcing_
                                                          * pow(reservation_price / Price(1.0), is->sector->parameters().consumption_price_elasticity)),
                                                    reservation_price);
        const Flow used_flow_U = Flow(std::min(desired_used_flow_U_tilde.get_quantity(), possible_used_flow_U_hat.get_quantity()), reservation_price);
        is->set_desired_used_flow_U_tilde(desired_used_flow_U_tilde);
        is->use_content_S(round(used_flow_U));
        region->add_consumption_flow_Y(round(used_flow_U));
        is->iterate_consumption_and_production();
    }
}

template<class ModelVariant>
void Consumer<ModelVariant>::iterate_expectation() {
    assertstep(EXPECTATION);
}

template<>
void Consumer<VariantBasic>::iterate_purchase() {
    assertstep(PURCHASE);
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_purchase();
    }
}

template<>
void Consumer<VariantDemand>::iterate_purchase() {
    assertstep(PURCHASE);
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_purchase();
    }
}

template<class ModelVariant>
void Consumer<ModelVariant>::iterate_purchase() {
    assertstep(PURCHASE);
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_purchase();
    }
}

template<>
void Consumer<VariantBasic>::iterate_investment() {}

template<>
void Consumer<VariantDemand>::iterate_investment() {}

template<>
void Consumer<VariantPrices>::iterate_investment() {}

template<class ModelVariant>
void Consumer<ModelVariant>::iterate_investment() {
    assertstep(INVESTMENT);
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_investment();
    }
}

#ifdef DEBUG
template<class ModelVariant>
void Consumer<ModelVariant>::print_details() const {
    info(std::string(*this) << ":");
    for (const auto& is : input_storages) {
        is->purchasing_manager->print_details();
    }
}
#endif

INSTANTIATE_BASIC(Consumer);
}  // namespace acclimate
