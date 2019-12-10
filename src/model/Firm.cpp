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

#include "model/Firm.h"
#include <vector>
#include "model/CapacityManagerPrices.h"
#include "model/EconomicAgent.h"
#include "model/GeoRoute.h"
#include "model/Model.h"
#include "model/PurchasingManagerPrices.h"
#include "model/Region.h"
#include "model/SalesManagerPrices.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "run.h"
#include "variants/VariantPrices.h"

namespace acclimate {

Firm::Firm(Sector* sector_p, Region* region_p, const Ratio& possible_overcapacity_ratio_beta_p)
        : EconomicAgent(sector_p, region_p, EconomicAgent::Type::FIRM),
          capacity_manager(new typename VariantPrices::CapacityManagerType(this, possible_overcapacity_ratio_beta_p)),
          sales_manager(new typename VariantPrices::SalesManagerType(this)) {}

inline Firm* Firm::as_firm() {
    return this;
}

inline const Firm* Firm::as_firm() const {
    return this;
}

void Firm::produce_X() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    production_X_ = capacity_manager->calc_production_X();
    assert(production_X_.get_quantity() >= 0.0);
    sector->add_production_X(production_X_);
}

void Firm::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    produce_X();
    for (const auto& is : input_storages) {
        Flow used_flow_U_current = round(production_X_ * is->get_technology_coefficient_a());
        if (production_X_.get_quantity() > 0.0) {
            used_flow_U_current.set_price(is->get_possible_use_U_hat().get_price());
        }
        is->use_content_S(used_flow_U_current);
        is->iterate_consumption_and_production();
    }
    sales_manager->distribute(production_X_);
}

void Firm::iterate_expectation() {
    assertstep(EXPECTATION);
    sales_manager->iterate_expectation();
    for (const auto& is : input_storages) {
        const FlowQuantity& desired_production =
                std::max(sales_manager->communicated_parameters().expected_production_X.get_quantity(),
                         sales_manager->sum_demand_requests_D().get_quantity());
        is->set_desired_used_flow_U_tilde(round(desired_production * is->get_technology_coefficient_a()));
    }
}

void Firm::add_initial_production_X_star(const Flow& initial_production_flow_X_star) {
    assertstep(INITIALIZATION);
    initial_production_X_star_ += initial_production_flow_X_star;
    production_X_ += initial_production_flow_X_star;
    sales_manager->add_initial_demand_request_D_star(initial_production_flow_X_star);
    sector->add_initial_production_X(initial_production_flow_X_star);
}

void Firm::subtract_initial_production_X_star(const Flow& initial_production_flow_X_star) {
    assertstep(INITIALIZATION);
    initial_production_X_star_ -= initial_production_flow_X_star;
    production_X_ -= initial_production_flow_X_star;
    sales_manager->subtract_initial_demand_request_D_star(initial_production_flow_X_star);
    sector->subtract_initial_production_X(initial_production_flow_X_star);
}

void Firm::add_initial_total_use_U_star(const Flow& initial_use_flow_U_star) {
    assertstep(INITIALIZATION);
    initial_total_use_U_star_ += initial_use_flow_U_star;
}

void Firm::subtract_initial_total_use_U_star(const Flow& initial_use_flow_U_star) {
    assertstep(INITIALIZATION);
    if (initial_total_use_U_star_.get_quantity() > initial_use_flow_U_star.get_quantity()) {
        initial_total_use_U_star_ -= initial_use_flow_U_star;
    } else {
        initial_total_use_U_star_ = Flow(0.0);
    }
}

void Firm::iterate_purchase() {
    assertstep(PURCHASE);
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_purchase();
    }
}

void Firm::iterate_investment() {
    // TODO: why was it doing nothing?
    // assertstep(INVESTMENT);
    // for (const auto& is : input_storages) {
    //     is->purchasing_manager->iterate_investment();
    // }
}

#ifdef DEBUG

void Firm::print_details() const {
    info(id() << ": X_star= " << initial_production_X_star_.get_quantity() << ":");
    for (auto it = input_storages.begin(); it != input_storages.end(); ++it) {
        (*it)->purchasing_manager->print_details();
    }
    sales_manager->print_details();
}

#endif

const Flow Firm::maximal_production_beta_X_star() const {
    return round(initial_production_X_star_ * capacity_manager->possible_overcapacity_ratio_beta);
}

const Flow Firm::forced_maximal_production_lambda_beta_X_star() const {
    return round(initial_production_X_star_ * forcing_ * capacity_manager->possible_overcapacity_ratio_beta);
}

const FlowQuantity Firm::maximal_production_quantity_beta_X_star() const {
    return round(initial_production_X_star_.get_quantity() * capacity_manager->possible_overcapacity_ratio_beta);
}

FloatType Firm::maximal_production_quantity_beta_X_star_float() const {
    return to_float(initial_production_X_star_.get_quantity() * capacity_manager->possible_overcapacity_ratio_beta);
}

const FlowQuantity Firm::forced_maximal_production_quantity_lambda_beta_X_star() const {
    return round(initial_production_X_star_.get_quantity() *
                 (capacity_manager->possible_overcapacity_ratio_beta * forcing_));
}

FloatType Firm::forced_maximal_production_quantity_lambda_beta_X_star_float() const {
    return to_float(initial_production_X_star_.get_quantity()) *
           (capacity_manager->possible_overcapacity_ratio_beta * forcing_);
}

const BusinessConnection* Firm::self_supply_connection() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    return self_supply_connection_.get();
}

void Firm::self_supply_connection(std::shared_ptr<BusinessConnection> self_supply_connection_p) {
    assertstep(INITIALIZATION);
    self_supply_connection_ = self_supply_connection_p;
}

const Flow& Firm::production_X() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    return production_X_;
}

}  // namespace acclimate
