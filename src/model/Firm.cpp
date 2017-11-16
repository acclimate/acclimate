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
#include "model/EconomicAgent.h"
#include "model/Model.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Firm<ModelVariant>::Firm(Sector<ModelVariant>* sector_p, Region<ModelVariant>* region_p, const Ratio& possible_overcapacity_ratio_beta_p)
    : EconomicAgent<ModelVariant>(sector_p, region_p, EconomicAgent<ModelVariant>::Type::FIRM),
      capacity_manager(new typename ModelVariant::CapacityManagerType(this, possible_overcapacity_ratio_beta_p)),
      sales_manager(new typename ModelVariant::SalesManagerType(this)) {}

template<class ModelVariant>
inline Firm<ModelVariant>* Firm<ModelVariant>::as_firm() {
    return this;
}

template<class ModelVariant>
inline const Firm<ModelVariant>* Firm<ModelVariant>::as_firm() const {
    return this;
}

template<class ModelVariant>
void Firm<ModelVariant>::produce_X() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    production_X_ = capacity_manager->calc_production_X();
    assert(production_X_.get_quantity() >= 0.0);
    sector->add_production_X(production_X_);
}

template<class ModelVariant>
void Firm<ModelVariant>::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    produce_X();
    for (const auto& is : input_storages) {
        is->use_content_S(round(production_X_ * is->get_technology_coefficient_a()));
        is->iterate_consumption_and_production();
    }
    sales_manager->distribute(production_X_);
}

template<>
void Firm<VariantPrices>::iterate_consumption_and_production() {
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

template<>
void Firm<VariantBasic>::iterate_expectation() {
    assertstep(EXPECTATION);
    sales_manager->iterate_expectation();
    for (const auto& is : input_storages) {
        is->set_desired_used_flow_U_tilde(round(capacity_manager->desired_production_X_tilde() * is->get_technology_coefficient_a()
                                                * forcing_  // Consider forcing to avoid buying goods that cannot be used when production is limited
                                                ));
    }
}

template<>
void Firm<VariantDemand>::iterate_expectation() {
    assertstep(EXPECTATION);
    sales_manager->iterate_expectation();
    for (const auto& is : input_storages) {
        is->set_desired_used_flow_U_tilde(round(capacity_manager->desired_production_X_tilde() * is->get_technology_coefficient_a()
                                                * forcing_  // Consider forcing to avoid buying goods that cannot be used when production is limited
                                                ));
    }
}

template<class ModelVariant>
void Firm<ModelVariant>::iterate_expectation() {
    assertstep(EXPECTATION);
    sales_manager->iterate_expectation();
    for (const auto& is : input_storages) {
        const FlowQuantity& desired_production =
            std::max(sales_manager->communicated_parameters().expected_production_X.get_quantity(), sales_manager->sum_demand_requests_D().get_quantity());
        is->set_desired_used_flow_U_tilde(round(desired_production * is->get_technology_coefficient_a()));
    }
}

template<class ModelVariant>
void Firm<ModelVariant>::add_initial_production_X_star(const Flow& initial_production_flow_X_star) {
    assertstep(INITIALIZATION);
    initial_production_X_star_ += initial_production_flow_X_star;
    production_X_ += initial_production_flow_X_star;
    sales_manager->add_initial_demand_request_D_star(initial_production_flow_X_star);
    sector->add_initial_production_X(initial_production_flow_X_star);
}

template<class ModelVariant>
void Firm<ModelVariant>::subtract_initial_production_X_star(const Flow& initial_production_flow_X_star) {
    assertstep(INITIALIZATION);
    initial_production_X_star_ -= initial_production_flow_X_star;
    production_X_ -= initial_production_flow_X_star;
    sales_manager->subtract_initial_demand_request_D_star(initial_production_flow_X_star);
    sector->subtract_initial_production_X(initial_production_flow_X_star);
}

template<class ModelVariant>
void Firm<ModelVariant>::add_initial_total_use_U_star(const Flow& initial_use_flow_U_star) {
    assertstep(INITIALIZATION);
    initial_total_use_U_star_ += initial_use_flow_U_star;
}

template<class ModelVariant>
void Firm<ModelVariant>::subtract_initial_total_use_U_star(const Flow& initial_use_flow_U_star) {
    assertstep(INITIALIZATION);
    if (initial_total_use_U_star_.get_quantity() > initial_use_flow_U_star.get_quantity()) {
        initial_total_use_U_star_ -= initial_use_flow_U_star;
    } else {
        initial_total_use_U_star_ = Flow(0.0);
    }
}

template<>
void Firm<VariantBasic>::iterate_purchase() {
    assertstep(PURCHASE);
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_purchase();
    }
}

template<>
void Firm<VariantDemand>::iterate_purchase() {
    assertstep(PURCHASE);
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_purchase();
    }
}

template<class ModelVariant>
void Firm<ModelVariant>::iterate_purchase() {
    assertstep(PURCHASE);
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_purchase();
    }
}

template<>
void Firm<VariantBasic>::iterate_investment() {}

template<>
void Firm<VariantDemand>::iterate_investment() {}

template<>
void Firm<VariantPrices>::iterate_investment() {}

template<class ModelVariant>
void Firm<ModelVariant>::iterate_investment() {
    assertstep(INVESTMENT);
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_investment();
    }
}

#ifdef DEBUG
template<class ModelVariant>
void Firm<ModelVariant>::print_details() const {
    info(std::string(*this) << ": X_star= " << initial_production_X_star_.get_quantity() << ":");
    for (auto it = input_storages.begin(); it != input_storages.end(); it++) {
        (*it)->purchasing_manager->print_details();
    }
    sales_manager->print_details();
}
#endif

INSTANTIATE_BASIC(Firm);
}  // namespace acclimate
