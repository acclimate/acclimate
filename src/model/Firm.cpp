// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/Firm.h"

#include "acclimate.h"
#include "model/CapacityManager.h"
#include "model/EconomicAgent.h"
#include "model/PurchasingManager.h"
#include "model/SalesManager.h"
#include "model/Sector.h"
#include "model/Storage.h"

namespace acclimate {

Firm::Firm(id_t id, Sector* sector_, Region* region, const Ratio& possible_overcapacity_ratio_beta)
    : EconomicAgent(std::move(id), region, EconomicAgent::type_t::FIRM),
      sector(sector_),
      capacity_manager(new CapacityManager(this, possible_overcapacity_ratio_beta)),
      sales_manager(new SalesManager(this)) {}

void Firm::initialize() {
    sales_manager->initialize();
    for (const auto& is : input_storages) {
        is->initialize();
    }
}

void Firm::produce() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    production_ = capacity_manager->calc_production();
    assert(production_.get_quantity() >= 0.0);
    sector->add_production(production_);
}

void Firm::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    produce();
    for (const auto& is : input_storages) {
        Flow used_flow = round(production_ * is->technology_coefficient());
        if (production_.get_quantity() > 0.0) {
            used_flow.set_price(is->get_possible_use().get_price());
        }
        is->use_content(used_flow);
        is->iterate_consumption_and_production();
    }
    sales_manager->distribute();
}

void Firm::iterate_expectation() {
    debug::assertstep(this, IterationStep::EXPECTATION);
    sales_manager->iterate_expectation();
    for (const auto& is : input_storages) {
        const FlowQuantity& desired_production =
            std::max(sales_manager->communicated_parameters().expected_production.get_quantity(), sales_manager->sum_demand_requests().get_quantity());
        is->set_desired_used_flow(round(desired_production * is->technology_coefficient()));
    }
}

void Firm::add_baseline_production(const Flow& flow) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    baseline_production_ += flow;
    production_ += flow;
    sales_manager->add_baseline_demand(flow);
    sector->add_baseline_production(flow);
}

void Firm::subtract_baseline_production(const Flow& flow) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    baseline_production_ -= flow;
    production_ -= flow;
    sales_manager->subtract_baseline_demand(flow);
    sector->subtract_baseline_production(flow);
}

void Firm::add_baseline_use(const Flow& flow) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    baseline_use_ += flow;
}

void Firm::subtract_baseline_use(const Flow& flow) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    if (baseline_use_.get_quantity() > flow.get_quantity()) {
        baseline_use_ -= flow;
    } else {
        baseline_use_ = Flow(0.0);
    }
}

void Firm::iterate_purchase() {
    debug::assertstep(this, IterationStep::PURCHASE);
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_purchase();
    }
}

void Firm::iterate_investment() {
    debug::assertstep(this, IterationStep::INVESTMENT);
    // TODO INVEST
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_investment();
    }
}

auto Firm::maximal_production() const -> Flow { return round(baseline_production_ * capacity_manager->possible_overcapacity_ratio_beta); }

auto Firm::forced_maximal_production_quantity() const -> FlowQuantity {
    return round(baseline_production_.get_quantity() * (capacity_manager->possible_overcapacity_ratio_beta * forcing_));
}

auto Firm::self_supply_connection() const -> const BusinessConnection* {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return self_supply_connection_.get();
}

void Firm::self_supply_connection(std::shared_ptr<BusinessConnection> self_supply_connection_p) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    self_supply_connection_ = std::move(self_supply_connection_p);
}

auto Firm::production() const -> const Flow& {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return production_;
}

void Firm::debug_print_details() const {
    if constexpr (Options::DEBUGGING) {
        log::info(this, "X_star= ", baseline_production_.get_quantity(), ":");
        for (const auto& input_storage : input_storages) {
            input_storage->purchasing_manager->debug_print_details();
        }
        sales_manager->debug_print_details();
    }
}

}  // namespace acclimate
