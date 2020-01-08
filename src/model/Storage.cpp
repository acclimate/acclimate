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

#include <algorithm>

#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/PurchasingManager.h"
#include "model/Sector.h"
#include "run.h"

namespace acclimate {

Storage::Storage(Sector* sector_p, EconomicAgent* economic_agent_p)
    : sector(sector_p), economic_agent(economic_agent_p), purchasing_manager(new PurchasingManager(this)) {}

void Storage::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    calc_content_S();
    input_flow_I_[2] = input_flow_I_[model()->other_register()];
    input_flow_I_[model()->other_register()] = Flow(0.0);
    purchasing_manager->iterate_consumption_and_production();
}

Flow Storage::estimate_possible_use_U_hat() const {
    assertstep(EXPECTATION);
    return content_S_ / model()->delta_t() + next_input_flow_I();
}

Flow Storage::get_possible_use_U_hat() const {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    return content_S_ / model()->delta_t() + current_input_flow_I();
}

const Flow& Storage::current_input_flow_I() const { return input_flow_I_[model()->other_register()]; }

const Flow& Storage::last_input_flow_I() const {
    assertstepor(OUTPUT, PURCHASE);
    return input_flow_I_[2];
}

Ratio Storage::get_technology_coefficient_a() const { return initial_input_flow_I_star_ / economic_agent->as_firm()->initial_production_X_star(); }

Ratio Storage::get_input_share_u() const { return initial_input_flow_I_star_ / economic_agent->as_firm()->initial_total_use_U_star(); }

void Storage::calc_content_S() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    assert(used_flow_U_.get_quantity() * model()->delta_t() <= content_S_.get_quantity() + current_input_flow_I().get_quantity() * model()->delta_t());
    auto former_content = content_S_;
    content_S_ = round(content_S_ + (current_input_flow_I() - used_flow_U_) * model()->delta_t());
    if (content_S_.get_quantity() <= model()->parameters().min_storage * initial_content_S_star_.get_quantity()) {
        model()->run()->event(EventType::STORAGE_UNDERRUN, sector, nullptr, economic_agent);
        Quantity quantity = model()->parameters().min_storage * initial_content_S_star_.get_quantity();
        content_S_ = Stock(quantity, quantity * former_content.get_price());
    }
    assert(content_S_.get_quantity() >= 0.0);

    Stock maxStock = initial_content_S_star_ * forcing_mu_ * sector->upper_storage_limit_omega;
    if (maxStock.get_quantity() < content_S_.get_quantity()) {
        model()->run()->event(EventType::STORAGE_OVERRUN, sector, nullptr, economic_agent, to_float(content_S_.get_quantity() - maxStock.get_quantity()));
        const Price tmp = content_S_.get_price();
        content_S_ = maxStock;
        content_S_.set_price(tmp);
    }
}

void Storage::use_content_S(const Flow& used_flow_U_current) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    used_flow_U_ = used_flow_U_current;
}

void Storage::set_desired_used_flow_U_tilde(const Flow& desired_used_flow_U_tilde_p) {
    if (economic_agent->is_consumer()) {
        assertstep(CONSUMPTION_AND_PRODUCTION);
    }
    if (economic_agent->is_firm()) {
        assertstep(EXPECTATION);
    }
    desired_used_flow_U_tilde_ = desired_used_flow_U_tilde_p;
}

void Storage::push_flow_Z(const Flow& flow_Z) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    input_flow_I_lock.call([&]() { input_flow_I_[model()->current_register()] += flow_Z; });
}

const Flow& Storage::next_input_flow_I() const {
    assertstep(EXPECTATION);
    return input_flow_I_[model()->current_register()];
}

void Storage::add_initial_flow_Z_star(const Flow& flow_Z_star) {
    assertstep(INITIALIZATION);
    input_flow_I_[1] += flow_Z_star;
    input_flow_I_[2] += flow_Z_star;
    initial_input_flow_I_star_ += flow_Z_star;  // == initial_used_flow_U_star
    initial_content_S_star_ = round(initial_content_S_star_ + flow_Z_star * sector->initial_storage_fill_factor_psi);
    content_S_ = round(content_S_ + flow_Z_star * sector->initial_storage_fill_factor_psi);
    purchasing_manager->add_initial_demand_D_star(flow_Z_star);
    if (economic_agent->type == EconomicAgent::Type::FIRM) {
        economic_agent->as_firm()->add_initial_total_use_U_star(flow_Z_star);
    }
}

bool Storage::subtract_initial_flow_Z_star(const Flow& flow_Z_star) {
    assertstep(INITIALIZATION);
    if (economic_agent->type == EconomicAgent::Type::FIRM) {
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

Model* Storage::model() const { return sector->model(); }

std::string Storage::id() const { return sector->id() + ":_S_->" + economic_agent->id(); }

const Stock& Storage::content_S() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    return content_S_;
}

const Flow& Storage::used_flow_U(const EconomicAgent* const caller) const {
    if constexpr (DEBUG_MODE) {
        if (caller != economic_agent) {
            assertstepnot(CONSUMPTION_AND_PRODUCTION);
        }
    } else {
        UNUSED(caller);
    }
    return used_flow_U_;
}

const Flow& Storage::desired_used_flow_U_tilde(const EconomicAgent* const caller) const {
    if (caller != economic_agent) {
        assertstepnot(CONSUMPTION_AND_PRODUCTION);
        assertstepnot(EXPECTATION);
    }
    return desired_used_flow_U_tilde_;
}

Parameters::StorageParameters& Storage::parameters_writable() {
    assertstep(INITIALIZATION);
    return parameters_;
}

}  // namespace acclimate
