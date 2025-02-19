// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/Storage.h"

#include "ModelRun.h"
#include "acclimate.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/PurchasingManager.h"
#include "model/Sector.h"

namespace acclimate {

Storage::Storage(Sector* sector_, EconomicAgent* economic_agent_)
    : sector(sector_), economic_agent(economic_agent_), purchasing_manager(new PurchasingManager(this)), id(sector->name() + "->" + economic_agent->name()) {}

Storage::~Storage() = default;  // needed to use forward declares for std::unique_ptr

void Storage::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    calc_content();
    input_flow_[2] = input_flow_[model()->other_register()];
    input_flow_[model()->other_register()] = Flow(0.0);
    purchasing_manager->iterate_consumption_and_production();
}

auto Storage::last_possible_use() const -> Flow {
    debug::assertstep(this, IterationStep::OUTPUT);
    return content_ / model()->delta_t() + last_input_flow();
}

auto Storage::estimate_possible_use() const -> Flow {
    debug::assertstep(this, IterationStep::EXPECTATION);
    return content_ / model()->delta_t() + next_input_flow();
}

auto Storage::get_possible_use() const -> Flow {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return content_ / model()->delta_t() + current_input_flow();
}

auto Storage::current_input_flow() const -> const Flow& { return input_flow_[model()->other_register()]; }

auto Storage::last_input_flow() const -> const Flow& {
    debug::assertstepor(this, IterationStep::OUTPUT, IterationStep::PURCHASE);
    return input_flow_[2];
}

auto Storage::get_input_share() const -> Ratio { return baseline_input_flow_ / economic_agent->as_firm()->baseline_use(); }

void Storage::calc_content() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    assert(used_flow_.get_quantity() * model()->delta_t() <= content_.get_quantity() + current_input_flow().get_quantity() * model()->delta_t());
    auto former_content = content_;
    content_ = round(content_ + (current_input_flow() - used_flow_) * model()->delta_t());
    if (content_.get_quantity() <= model()->parameters().min_storage * baseline_content_.get_quantity()) {
        model()->run()->event(EventType::STORAGE_UNDERRUN, sector, economic_agent);
        Quantity const quantity = model()->parameters().min_storage * baseline_content_.get_quantity();
        content_ = Stock(quantity, quantity * former_content.get_price());
    }
    assert(content_.get_quantity() >= 0.0);

    Stock const maximum_content = baseline_content_ * forcing_ * sector->upper_storage_limit;
    if (maximum_content.get_quantity() < content_.get_quantity()) {
        model()->run()->event(EventType::STORAGE_OVERRUN, sector, economic_agent, to_float(content_.get_quantity() - maximum_content.get_quantity()));
        const Price tmp = content_.get_price();
        content_ = maximum_content;
        content_.set_price(tmp);
    }
}

void Storage::use_content(const Flow& flow) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    used_flow_ = flow;
}

void Storage::set_desired_used_flow(const Flow& flow) {
    if (economic_agent->is_consumer()) {
        debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    }
    if (economic_agent->is_firm()) {
        debug::assertstep(this, IterationStep::EXPECTATION);
    }
    desired_used_flow_ = flow;
}

void Storage::push_flow(const AnnotatedFlow& flow) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    input_flow_lock_.call([&]() { input_flow_[model()->current_register()] += flow.current; });
    last_delivery_baseline_ = flow.baseline;
}

auto Storage::next_input_flow() const -> const Flow& {
    debug::assertstep(this, IterationStep::EXPECTATION);
    return input_flow_[model()->current_register()];
}

void Storage::add_baseline_flow(const Flow& flow) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    input_flow_[1] += flow;
    input_flow_[2] += flow;
    baseline_input_flow_ += flow;
    last_delivery_baseline_ += flow.get_quantity();
    baseline_content_ = round(baseline_content_ + flow * sector->baseline_storage_fill_factor);
    content_ = round(content_ + flow * sector->baseline_storage_fill_factor);
    purchasing_manager->add_baseline_demand(flow);
    if (economic_agent->type == EconomicAgent::type_t::FIRM) {
        economic_agent->as_firm()->add_baseline_use(flow);
    }
}

auto Storage::subtract_baseline_flow(const Flow& flow) -> bool {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    if (economic_agent->type == EconomicAgent::type_t::FIRM) {
        economic_agent->as_firm()->subtract_baseline_use(flow);
    }
    if (baseline_input_flow_.get_quantity() - flow.get_quantity() >= FlowQuantity::precision) {
        input_flow_[1] -= flow;
        input_flow_[2] -= flow;
        baseline_input_flow_ -= flow;
        last_delivery_baseline_ -= flow.get_quantity();
        baseline_content_ = round(baseline_content_ - flow * sector->baseline_storage_fill_factor);
        content_ = round(content_ - flow * sector->baseline_storage_fill_factor);
        purchasing_manager->subtract_baseline_demand(flow);
        return false;
    }
    economic_agent->input_storages.remove(this);
    return true;
}

auto Storage::model() -> Model* { return sector->model(); }
auto Storage::model() const -> const Model* { return sector->model(); }

auto Storage::content() const -> const Stock& {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return content_;
}

auto Storage::used_flow(const EconomicAgent* caller) const -> const Flow& {
    if constexpr (Options::DEBUGGING) {
        if (caller != economic_agent) {
            debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
        }
    }
    return used_flow_;
}

auto Storage::desired_used_flow(const EconomicAgent* caller) const -> const Flow& {
    if constexpr (Options::DEBUGGING) {
        if (caller != economic_agent) {
            debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
            debug::assertstepnot(this, IterationStep::EXPECTATION);
        }
    }
    return desired_used_flow_;
}

void Storage::calculate_and_set_technological_coefficient() {
    if (economic_agent->is_firm()) {
        technology_coefficient_ = baseline_input_flow_ / economic_agent->as_firm()->baseline_production();
    }
}

}  // namespace acclimate
