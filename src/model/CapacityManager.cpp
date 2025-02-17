// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/CapacityManager.h"

#include "acclimate.h"
#include "model/Firm.h"
#include "model/PurchasingManager.h"
#include "model/SalesManager.h"
#include "model/Storage.h"

namespace acclimate {

CapacityManager::CapacityManager(Firm* firm_, Ratio possible_overcapacity_ratio_beta_)
    : firm(firm_), possible_overcapacity_ratio_beta(possible_overcapacity_ratio_beta_) {}

auto CapacityManager::get_production_capacity() const -> Ratio { return firm->production() / firm->baseline_production(); }

auto CapacityManager::get_desired_production_capacity() const -> Ratio { return desired_production_ / firm->baseline_production(); }

auto CapacityManager::get_possible_production_capacity() const -> Ratio { return possible_production_ / firm->baseline_production(); }

void CapacityManager::calc_possible_and_desired_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    possible_production_ = get_possible_production();
    desired_production_ = firm->sales_manager->sum_demand_requests();
}

auto CapacityManager::model() const -> const Model* { return firm->model(); }

auto CapacityManager::name() const -> std::string { return firm->name(); }

void CapacityManager::debug_print_inputs() const {
    if constexpr (Options::DEBUGGING) {
        log::info(this, firm->input_storages.size(), " inputs:");
        for (const auto& is : firm->input_storages) {
            Flow const possible_use = is->get_possible_use();
            log::info("    ", is->name(), ":",                                                                     //
                      "  U_hat= ", std::setw(11), possible_use.get_quantity(),                                     //
                      "  n_hat= ", std::setw(11), possible_use.get_price(),                                        //
                      "  n_hat*a= ", std::setw(11), (possible_use.get_price() * is->get_technology_coefficient())  //
            );
        }
    }
}

auto CapacityManager::get_possible_production_intern(bool consider_transport_in_production_costs, bool estimate) const -> Flow {
    debug::assertstepor(this, IterationStep::CONSUMPTION_AND_PRODUCTION, IterationStep::EXPECTATION);
    Ratio possible_production_capacity = firm->forcing() * possible_overcapacity_ratio_beta;
    auto unit_commodity_costs = Price(0.0);

    for (const auto& input_storage : firm->input_storages) {
        Flow possible_use(0.0);
        if (estimate) {
            possible_use = input_storage->estimate_possible_use();
        } else {
            possible_use = input_storage->get_possible_use();
        }
        if (consider_transport_in_production_costs) {
            Flow const transport_flow = input_storage->purchasing_manager->get_transport_flow();
            unit_commodity_costs += (possible_use + transport_flow).get_price() * input_storage->get_technology_coefficient();
        } else {
            unit_commodity_costs += possible_use.get_price() * input_storage->get_technology_coefficient();
        }
        Ratio const tmp = possible_use / input_storage->baseline_used_flow();
        if (tmp < possible_production_capacity) {
            possible_production_capacity = tmp;
        }
    }
    assert(possible_production_capacity >= 0.0);
    Flow result = round(firm->baseline_production() * possible_production_capacity);
    // note: if result.get_quantity() == 0.0 price is NAN
    if (result.get_quantity() > 0.0) {
        result.set_price(round(unit_commodity_costs + firm->sales_manager->get_baseline_unit_variable_production_costs()));
    }
    return result;
}

auto CapacityManager::get_possible_production() const -> Flow {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    bool const consider_transport_in_production_costs = false;
    return get_possible_production_intern(consider_transport_in_production_costs, false);
}

auto CapacityManager::estimate_possible_production() const -> Flow {
    debug::assertstep(this, IterationStep::EXPECTATION);
    bool const consider_transport_in_production_costs = true;
    return get_possible_production_intern(consider_transport_in_production_costs, true);
}

auto CapacityManager::calc_production() -> Flow {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    calc_possible_and_desired_production();
    return firm->sales_manager->calc_production();
}

}  // namespace acclimate
