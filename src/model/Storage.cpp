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

#include "model/Storage.h"
#include "model/Model.h"
#include "model/Sector.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Storage<ModelVariant>::Storage(Sector<ModelVariant>* sector_p, EconomicAgent<ModelVariant>* economic_agent_p)
    : sector(sector_p), economic_agent(economic_agent_p), purchasing_manager(new typename ModelVariant::PurchasingManagerType(this)) {}

template<class ModelVariant>
void Storage<ModelVariant>::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    calc_content_S();
    input_flow_I_[2] = input_flow_I_[sector->model->other_register()];
    input_flow_I_[sector->model->other_register()] = Flow(0.0);
    purchasing_manager->iterate_consumption_and_production();
}

template<class ModelVariant>
const Flow Storage<ModelVariant>::get_possible_use_U_hat() const {
    assertstepor(CONSUMPTION_AND_PRODUCTION, EXPECTATION);
    return content_S_ / sector->model->delta_t() + current_input_flow_I();
}

template<class ModelVariant>
const Flow& Storage<ModelVariant>::current_input_flow_I() const {
    return input_flow_I_[sector->model->other_register()];
}

template<class ModelVariant>
const Flow& Storage<ModelVariant>::last_input_flow_I() const {
    assertstepor(OUTPUT, PURCHASE);
    return input_flow_I_[2];
}

template<class ModelVariant>
Ratio Storage<ModelVariant>::get_technology_coefficient_a() const {
    return initial_input_flow_I_star_ / economic_agent->as_firm()->initial_production_X_star();
}

template<class ModelVariant>
Ratio Storage<ModelVariant>::get_input_share_u() const {
    return initial_input_flow_I_star_ / economic_agent->as_firm()->initial_total_use_U_star();
}

#ifdef VARIANT_PRICES
template<>
void Storage<VariantPrices>::calc_content_S() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    content_S_ = round(content_S_ + (current_input_flow_I() - used_flow_U_) * sector->model->delta_t());
    if (sector->model->parameters().min_storage > 0.0) {
        Quantity quantity = std::max(sector->model->parameters().min_storage * initial_content_S_star_.get_quantity(), content_S_.get_quantity());
        content_S_ = Stock(quantity, quantity * content_S_.get_price());
    }
    assert(content_S_.get_quantity() >= 0.0);

    Stock maxStock = initial_content_S_star_ * forcing_mu_ * sector->upper_storage_limit_omega;
    if (maxStock.get_quantity() < content_S_.get_quantity()) {
        Acclimate::Run<VariantPrices>::instance()->event(EventType::STORAGE_OVERRUN, sector, nullptr, economic_agent,
                                                         to_float(content_S_.get_quantity() - maxStock.get_quantity()));
        const Price tmp = content_S_.get_price();
        content_S_ = maxStock;
        content_S_.set_price(tmp);
    }
    if (content_S_.get_quantity() <= 0.0) {
        Acclimate::Run<VariantPrices>::instance()->event(EventType::STORAGE_EMPTY, sector, nullptr, economic_agent);
    }
}
#endif

template<class ModelVariant>
void Storage<ModelVariant>::calc_content_S() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    content_S_ = round(content_S_ + (current_input_flow_I() - used_flow_U_) * sector->model->delta_t());
    assert(content_S_.get_quantity() >= 0.0);

    Stock maxStock = initial_content_S_star_ * forcing_mu_ * sector->upper_storage_limit_omega;
    if (maxStock.get_quantity() < content_S_.get_quantity()) {
        Acclimate::Run<ModelVariant>::instance()->event(EventType::STORAGE_OVERRUN, sector, nullptr, economic_agent,
                                                        to_float(content_S_.get_quantity() - maxStock.get_quantity()));
        content_S_ = maxStock;
    }
    if (content_S_.get_quantity() <= 0.0) {
        Acclimate::Run<ModelVariant>::instance()->event(EventType::STORAGE_EMPTY, sector, nullptr, economic_agent);
    }
}

// calculates used content from storage
template<class ModelVariant>
void Storage<ModelVariant>::use_content_S(const Flow& used_flow_U_current) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    used_flow_U_ = used_flow_U_current;
}

// calculates used content from storage
template<class ModelVariant>
void Storage<ModelVariant>::set_desired_used_flow_U_tilde(const Flow& desired_used_flow_U_tilde_p) {
    if (economic_agent->is_consumer()) {
        assertstep(CONSUMPTION_AND_PRODUCTION);
    }
    if (economic_agent->is_firm()) {
        assertstep(EXPECTATION);
    }
    desired_used_flow_U_tilde_ = desired_used_flow_U_tilde_p;
}

// push flow_Z to input_flow_I
template<class ModelVariant>
void Storage<ModelVariant>::push_flow_Z(const Flow& flow_Z) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
#pragma omp critical(input_flow_I_)
    { input_flow_I_[sector->model->current_register()] += flow_Z; }
}

template<class ModelVariant>
const Flow& Storage<ModelVariant>::next_input_flow_I() const {
    assertstep(UNDEFINED);
    return input_flow_I_[sector->model->current_register()];
}

template<class ModelVariant>
void Storage<ModelVariant>::add_initial_flow_Z_star(const Flow& flow_Z_star) {
    assertstep(INITIALIZATION);
    input_flow_I_[1] += flow_Z_star;
    input_flow_I_[2] += flow_Z_star;
    initial_input_flow_I_star_ += flow_Z_star;  // == initial_used_flow_U_star
    initial_content_S_star_ = round(initial_content_S_star_ + flow_Z_star * sector->initial_storage_fill_factor_psi);
    content_S_ = round(content_S_ + flow_Z_star * sector->initial_storage_fill_factor_psi);
    purchasing_manager->add_initial_demand_D_star(flow_Z_star);
    if (economic_agent->type == EconomicAgent<ModelVariant>::Type::FIRM) {
        economic_agent->as_firm()->add_initial_total_use_U_star(flow_Z_star);
    }
}

template<class ModelVariant>
bool Storage<ModelVariant>::subtract_initial_flow_Z_star(const Flow& flow_Z_star) {
    assertstep(INITIALIZATION);
    if (economic_agent->type == EconomicAgent<ModelVariant>::Type::FIRM) {
        economic_agent->as_firm()->subtract_initial_total_use_U_star(flow_Z_star);
    }
    if (initial_input_flow_I_star_.get_quantity() - flow_Z_star.get_quantity() >= FlowQuantity::precision) {
        input_flow_I_[1] -= flow_Z_star;
        input_flow_I_[2] -= flow_Z_star;
        initial_input_flow_I_star_ -= flow_Z_star;  // = initial_used_flow_U_star
        initial_content_S_star_ = round(initial_content_S_star_ - flow_Z_star * sector->initial_storage_fill_factor_psi);
        content_S_ = round(content_S_ - flow_Z_star * sector->initial_storage_fill_factor_psi);
        purchasing_manager->subtract_initial_demand_D_star(flow_Z_star);
        return false;
    }
    economic_agent->remove_storage(this);
    return true;
}

INSTANTIATE_BASIC(Storage);
}  // namespace acclimate
