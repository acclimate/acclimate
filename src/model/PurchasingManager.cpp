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

#include "model/PurchasingManager.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <utility>

#include "ModelRun.h"
#include "acclimate.h"
#include "model/BusinessConnection.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/SalesManager.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "optimization.h"
#include "parameters.h"

static constexpr auto MAX_GRADIENT = 1e3;
static constexpr bool IGNORE_ROUNDOFFLIMITED = true;

namespace acclimate {

PurchasingManager::PurchasingManager(Storage* storage_p) : storage(storage_p) {}

void PurchasingManager::iterate_consumption_and_production() { debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION); }

bool PurchasingManager::remove_business_connection(const BusinessConnection* business_connection) {
    auto it = std::find_if(std::begin(business_connections), std::end(business_connections),
                           [business_connection](const auto& bc) { return bc.get() == business_connection; });
    if (it == std::end(business_connections)) {
        throw log::error(this, "Business connection ", business_connection->name(), " not found");
    }
    business_connections.erase(it);
    if (business_connections.empty()) {
        storage->economic_agent->input_storages.remove(storage);
        return true;
    }
    return false;
}

FlowQuantity PurchasingManager::get_flow_deficit() const {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return round(std::accumulate(std::begin(business_connections), std::end(business_connections), FlowQuantity(0.0),
                                 [](FlowQuantity q, const auto& bc) { return std::move(q) + bc->get_flow_deficit(); }));
}

Flow PurchasingManager::get_transport_flow() const {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return std::accumulate(std::begin(business_connections), std::end(business_connections), Flow(0.0),
                           [](const Flow& f, const auto& bc) { return f + bc->get_transport_flow(); });
}

Flow PurchasingManager::get_disequilibrium() const {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (const auto& bc : business_connections) {
        res.add_possibly_negative(bc->get_disequilibrium());
    }
    return res;
}

FloatType PurchasingManager::get_stddeviation() const {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return std::accumulate(std::begin(business_connections), std::end(business_connections), 0.0,
                           [](FloatType v, const auto& bc) { return v + bc->get_stddeviation(); });
}

Flow PurchasingManager::get_sum_of_last_shipments() const {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return std::accumulate(std::begin(business_connections), std::end(business_connections), Flow(0.0),
                           [](const Flow& f, const auto& bc) { return f + bc->last_shipment_Z(); });
}

PurchasingManager::~PurchasingManager() {
    for (auto& bc : business_connections) {
        bc->buyer.invalidate();
    }
}

const Demand& PurchasingManager::demand_D(const EconomicAgent* caller) const {
    if constexpr (options::DEBUGGING) {
        if (caller != storage->economic_agent) {
            debug::assertstepnot(this, IterationStep::PURCHASE);
        }
    }
    return demand_D_;
}

const Model* PurchasingManager::model() const { return storage->model(); }
Model* PurchasingManager::model() { return storage->model(); }

std::string PurchasingManager::name() const { return storage->sector->name() + "->" + storage->economic_agent->name(); }

FloatType PurchasingManager::optimized_value() const {
    debug::assertstepnot(this, IterationStep::PURCHASE);
    return optimized_value_;
}

Demand PurchasingManager::storage_demand() const {
    debug::assertstepnot(this, IterationStep::PURCHASE);
    return demand_D_ - purchase_;
}

const Demand& PurchasingManager::purchase() const {
    debug::assertstepnot(this, IterationStep::PURCHASE);
    return purchase_;
}

const FlowValue& PurchasingManager::total_transport_penalty() const {
    debug::assertstepnot(this, IterationStep::PURCHASE);
    return total_transport_penalty_;
}

const FlowValue& PurchasingManager::expected_costs(const EconomicAgent* caller) const {
    if constexpr (options::DEBUGGING) {
        if (caller != storage->economic_agent) {
            debug::assertstepnot(this, IterationStep::PURCHASE);
        }
    }
    return expected_costs_;
}

FloatType PurchasingManager::estimate_production_extension_penalty(const BusinessConnection* bc, FloatType production_quantity_X) const {
    assert(production_quantity_X >= 0.0);
    if (production_quantity_X <= bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float()) {  // not in production extension
        return 0.0;
    }
    // in production extension
    return std::max(0.0, to_float(bc->seller->firm->sector->parameters().estimated_price_increase_production_extension)
                             / (2 * bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float())
                             * (production_quantity_X - bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float())
                             * (production_quantity_X - bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float()));
}

FloatType PurchasingManager::estimate_marginal_production_costs(const BusinessConnection* bc,
                                                                FloatType production_quantity_X,
                                                                FloatType unit_production_costs_n_c) const {
    assert(production_quantity_X >= 0.0);
    assert(!std::isnan(unit_production_costs_n_c));
    if (production_quantity_X <= bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float()) {  // not in production extension
        return unit_production_costs_n_c;
    }
    // in production extension
    return unit_production_costs_n_c + estimate_marginal_production_extension_penalty(bc, production_quantity_X);
}

FloatType PurchasingManager::estimate_marginal_production_extension_penalty(const BusinessConnection* bc, FloatType production_quantity_X) const {
    assert(production_quantity_X >= 0.0);
    if (production_quantity_X <= bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float()) {  // not in production extension
        return 0.0;
    }
    // in production extension
    return to_float(bc->seller->firm->sector->parameters().estimated_price_increase_production_extension)
           / bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float()
           * (production_quantity_X - bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float());
}

inline FloatType PurchasingManager::scaled_D_r(FloatType D_r, const BusinessConnection* bc) const { return D_r / partial_D_r_scaled_D_r(bc); }

inline FloatType PurchasingManager::unscaled_D_r(FloatType x, const BusinessConnection* bc) const { return x * partial_D_r_scaled_D_r(bc); }

inline FloatType PurchasingManager::partial_D_r_scaled_D_r(const BusinessConnection* bc) { return to_float(bc->initial_flow_Z_star().get_quantity()); }

inline FloatType PurchasingManager::scaled_objective(FloatType obj) const { return obj / partial_objective_scaled_objective(); }

inline FloatType PurchasingManager::unscaled_objective(FloatType x) const { return x * partial_objective_scaled_objective(); }

inline FloatType PurchasingManager::partial_objective_scaled_objective() const { return to_float(storage->initial_used_flow_U_star().get_quantity()); }

inline FloatType PurchasingManager::scaled_use(FloatType use) const { return use / partial_use_scaled_use(); }

inline FloatType PurchasingManager::unscaled_use(FloatType x) const { return x * partial_use_scaled_use(); }

inline FloatType PurchasingManager::partial_use_scaled_use() const { return to_float(storage->initial_used_flow_U_star().get_quantity()); }

FloatType PurchasingManager::equality_constraint(const double* x, double* grad) const {
    FloatType use = 0.0;
    for (std::size_t r = 0; r < purchasing_connections.size(); ++r) {
        const auto D_r = unscaled_D_r(x[r], purchasing_connections[r]);
        assert(!std::isnan(D_r));
        use += D_r;
        if (grad != nullptr) {
            grad[r] = -partial_D_r_scaled_D_r(purchasing_connections[r]) / partial_use_scaled_use();
            if constexpr (options::OPTIMIZATION_WARNINGS) {
                if (grad[r] > MAX_GRADIENT) {
                    log::warning(this, purchasing_connections[r]->name(), ": large gradient of ", grad[r]);
                }
            }
        }
    }
    return scaled_use(to_float(desired_purchase_) - use);
}

FloatType PurchasingManager::max_objective(const double* x, double* grad) const {
    FloatType costs = 0.0;
    FloatType purchase = 0.0;
    for (std::size_t r = 0; r < purchasing_connections.size(); ++r) {
        const auto D_r = unscaled_D_r(x[r], purchasing_connections[r]);
        assert(!std::isnan(D_r));
        costs += n_r(D_r, purchasing_connections[r]) * D_r + transport_penalty(D_r, purchasing_connections[r]);
        purchase += D_r;
        if (grad != nullptr) {
            grad[r] = -partial_D_r_scaled_D_r(purchasing_connections[r])
                      * (grad_n_r(D_r, purchasing_connections[r]) * D_r + n_r(D_r, purchasing_connections[r])
                         + partial_D_r_transport_penalty(D_r, purchasing_connections[r]))
                      / partial_objective_scaled_objective();
            if constexpr (options::OPTIMIZATION_WARNINGS) {
                if (grad[r] > MAX_GRADIENT) {
                    log::warning(this, purchasing_connections[r]->name(), ": large gradient of ", grad[r]);
                }
            }
        }
    }
    return scaled_objective(-costs);
}

FloatType PurchasingManager::expected_average_price_E_n_r(FloatType D_r, const BusinessConnection* business_connection) const {
    const auto X_expected = expected_production(business_connection);
    auto X_new = D_r + expected_additional_production(business_connection);
    assert(round(FlowQuantity(X_new)) <= business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity());
    if (X_new > to_float(business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity())) {
        if constexpr (options::OPTIMIZATION_WARNINGS) {
            log::warning(this, "X_new > X_hat: X_new = ", FlowQuantity(X_new),
                         " X_hat = ", business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity());
        }
        X_new = to_float(business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity());
    }

    FloatType new_price_demand_request = 0.0;
    if (X_expected <= 0.0 && X_new <= 0.0) {  // no production in last timestep
        new_price_demand_request = to_float(business_connection->seller->communicated_parameters().offer_price_n_bar);
    } else if (X_expected <= 0.0 && X_new > 0.0) {
        new_price_demand_request = to_float(business_connection->seller->communicated_parameters().offer_price_n_bar)
                                   + estimate_production_extension_penalty(business_connection, X_new) / X_new;
    } else if (X_expected > 0.0 && X_new <= 0.0) {
        assert(estimate_production_extension_penalty(business_connection, X_expected) / X_expected
               < to_float(business_connection->seller->communicated_parameters().offer_price_n_bar));
        new_price_demand_request = to_float(business_connection->seller->communicated_parameters().offer_price_n_bar)
                                   - estimate_production_extension_penalty(business_connection, X_expected) / X_expected;
        assert(new_price_demand_request > 0.0);
    } else {  // regular case
        new_price_demand_request = to_float(business_connection->seller->communicated_parameters().offer_price_n_bar)
                                   - estimate_production_extension_penalty(business_connection, X_expected) / X_expected
                                   + estimate_production_extension_penalty(business_connection, X_new) / X_new;
    }
    assert(!std::isnan(new_price_demand_request));
    assert(new_price_demand_request > 0.0);
    return new_price_demand_request;
}

FloatType PurchasingManager::n_r(FloatType D_r, const BusinessConnection* business_connection) const {
    assert(D_r >= 0.0);
    const auto n_bar = to_float(business_connection->seller->communicated_parameters().offer_price_n_bar);
    const auto X_expected = expected_production(business_connection);
    const auto additional_X_expected = expected_additional_production(business_connection);
    const auto E_n_r = expected_average_price_E_n_r(D_r, business_connection);
    const auto D_r_min = std::max(0.0, business_connection->seller->firm->forced_initial_production_quantity_lambda_X_star_float() - additional_X_expected);
    const auto X_new_min = D_r_min + additional_X_expected;
    assert(X_new_min > 0.0);
    const auto npe_at_X_expected = (X_expected > 0.0) ? estimate_production_extension_penalty(business_connection, X_expected) / X_expected : 0.0;
    const auto npe_at_X_new_min = (X_new_min > 0.0) ? estimate_production_extension_penalty(business_connection, X_new_min) / X_new_min : 0.0;
    const auto n_bar_min = n_bar - npe_at_X_expected + npe_at_X_new_min;
    const auto n_co = calc_n_co(n_bar_min, D_r_min, business_connection);
    if (n_co <= n_bar_min) {  // in linear regime of E_n(D) curve
        if (D_r < D_r_min) {  // note: D_r_min == 0 cannot occur
            assert(D_r_min > 0.0);
            return n_co + (n_bar_min - n_co) / D_r_min * D_r;
        }
        // in production extension of E_n(D) curve
        return E_n_r;
    }
    // E_n(D) curved cropped from below with n_co
    if (E_n_r <= n_co) {
        return n_co;
    }
    return E_n_r;
}

FloatType PurchasingManager::grad_expected_average_price_E_n_r(FloatType D_r, const BusinessConnection* business_connection) const {
    auto X_new = D_r + expected_additional_production(business_connection);
    assert(round(FlowQuantity(X_new)) <= business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity());
    if (X_new > to_float(business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity())) {
        if constexpr (options::OPTIMIZATION_WARNINGS) {
            log::warning(this, "X_new > X_hat: X_new = ", FlowQuantity(X_new),
                         " X_hat = ", business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity());
        }
        X_new = to_float(business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity());
    }
    if (X_new <= 0.0) {
        return 0.0;
    }
    // regular case
    return estimate_marginal_production_extension_penalty(business_connection, X_new) / X_new
           - estimate_production_extension_penalty(business_connection, X_new) / X_new / X_new;
}

FloatType PurchasingManager::calc_n_co(FloatType n_bar_min, FloatType D_r_min, const BusinessConnection* business_connection) const {
    const auto n_co =
        estimate_marginal_production_costs(business_connection, to_float(business_connection->seller->communicated_parameters().production_X.get_quantity()),
                                           business_connection->seller->communicated_parameters().possible_production_X_hat.get_price_float());
    if (model()->parameters().maximal_decrease_reservation_price_limited_by_markup) {
        const auto n_crit = n_bar_min - to_float(storage->sector->parameters().initial_markup) * D_r_min;
        return std::max(n_co, n_crit);
    }
    return n_co;
}

inline FloatType PurchasingManager::expected_production(const BusinessConnection* business_connection) {
    const auto X_expected = to_float(business_connection->seller->communicated_parameters().expected_production_X.get_quantity());
    if constexpr (options::USE_MIN_PASSAGE_IN_EXPECTATION) {
        return business_connection->get_minimum_passage() * X_expected;
    } else {
        return X_expected;
    }
}

inline FloatType PurchasingManager::expected_additional_production(const BusinessConnection* business_connection) {
    const auto X = to_float(business_connection->seller->communicated_parameters().production_X.get_quantity());
    const auto X_expected = expected_production(business_connection);
    const auto Z_last = to_float(business_connection->last_shipment_Z().get_quantity());
    const Ratio ratio_X_expected_to_X = (X > 0.0) ? X_expected / X : 0.0;
    return ratio_X_expected_to_X * (X - Z_last);
}

FloatType PurchasingManager::grad_n_r(FloatType D_r, const BusinessConnection* business_connection) const {
    const auto n_bar = to_float(business_connection->seller->communicated_parameters().offer_price_n_bar);
    const auto X_expected = expected_production(business_connection);
    const auto additional_X_expected = expected_additional_production(business_connection);
    const auto grad_E_n_r = grad_expected_average_price_E_n_r(D_r, business_connection);
    const auto E_n_r = expected_average_price_E_n_r(D_r, business_connection);
    const auto D_r_min = std::max(0.0,
                                  business_connection->seller->firm->forced_initial_production_quantity_lambda_X_star_float()
                                      - additional_X_expected);  // note: D_r_star is the demand request for X_new=lambda * X_star
    assert(D_r_min >= 0.0);
    const auto X_new_min = D_r_min + additional_X_expected;
    assert(X_new_min > 0.0);
    const auto npe_at_X_expected = (X_expected > 0.0) ? estimate_production_extension_penalty(business_connection, X_expected) / X_expected : 0.0;
    const auto npe_at_X_new_min = (X_new_min > 0.0) ? estimate_production_extension_penalty(business_connection, X_new_min) / X_new_min : 0.0;
    const auto n_bar_min = n_bar - npe_at_X_expected + npe_at_X_new_min;
    const auto n_co = calc_n_co(n_bar_min, D_r_min, business_connection);
    if (n_co <= n_bar_min) {  // in linear regime of E_n(D) curve
        if (D_r < D_r_min && D_r_min > 0.0) {
            return (n_bar_min - n_co) / D_r_min;
        }
        // in production extension of E_n(D) curve
        return grad_E_n_r;
    }
    // E_n(D) curved cropped from below with n_co
    if (E_n_r <= n_co) {
        return 0.0;
    }
    return grad_E_n_r;
}

FloatType PurchasingManager::transport_penalty(FloatType D_r, const BusinessConnection* business_connection) const {
    FloatType target = 0.0;
    if (model()->parameters().deviation_penalty) {
        target = to_float(business_connection->last_demand_request_D(this).get_quantity());
    } else {
        target = to_float(business_connection->initial_flow_Z_star().get_quantity());
    }
    if (model()->parameters().quadratic_transport_penalty) {
        FloatType marg_penalty = 0.0;
        if (D_r < target) {
            marg_penalty = -to_float(storage->sector->parameters().initial_markup);
        } else if (D_r > target) {
            marg_penalty = to_float(storage->sector->parameters().initial_markup);
        } else {
            marg_penalty = 0.0;
        }
        if (model()->parameters().relative_transport_penalty) {
            if (target > FlowQuantity::precision) {
                return (D_r - target) * ((D_r - target) * to_float(model()->parameters().transport_penalty_large) / (target * target) / 2 + marg_penalty);
            }
            return D_r * D_r * to_float(model()->parameters().transport_penalty_large / 2 + Price(marg_penalty));
        }
        return (D_r - target) * ((D_r - target) * to_float(model()->parameters().transport_penalty_large) / 2 + marg_penalty);
    }
    if (model()->parameters().relative_transport_penalty) {
        return partial_D_r_transport_penalty(D_r, business_connection) * (D_r - target) / target;
    }
    return partial_D_r_transport_penalty(D_r, business_connection) * (D_r - target);
}

FloatType PurchasingManager::partial_D_r_transport_penalty(FloatType D_r, const BusinessConnection* business_connection) const {
    FloatType target = 0.0;
    if (model()->parameters().deviation_penalty) {
        target = to_float(business_connection->last_demand_request_D(this).get_quantity());
    } else {
        target = to_float(business_connection->initial_flow_Z_star().get_quantity());
    }
    if (model()->parameters().quadratic_transport_penalty) {
        FloatType marg_penalty = 0.0;
        if (D_r < target) {
            marg_penalty = -to_float(storage->sector->parameters().initial_markup);
        } else if (D_r > target) {
            marg_penalty = to_float(storage->sector->parameters().initial_markup);
        } else {
            marg_penalty = 0.0;
        }
        if (model()->parameters().relative_transport_penalty) {
            if (target > FlowQuantity::precision) {
                return (D_r - target) * to_float(model()->parameters().transport_penalty_large) / (target * target) + marg_penalty;
            }
            return D_r * to_float(model()->parameters().transport_penalty_large) + marg_penalty;
        }
        return (D_r - target) * to_float(model()->parameters().transport_penalty_large) + marg_penalty;
    }
    if (model()->parameters().relative_transport_penalty) {
        if (D_r < target) {
            return -to_float(model()->parameters().transport_penalty_small) / target;
        }
        if (D_r > target) {
            return to_float(model()->parameters().transport_penalty_large) / target;
        }
        return (to_float(model()->parameters().transport_penalty_large) - to_float(model()->parameters().transport_penalty_small)) / 2 / target;
    }
    if (D_r < target) {
        return -to_float(model()->parameters().transport_penalty_small);
    }
    if (D_r > target) {
        return to_float(model()->parameters().transport_penalty_large);
    }
    return (to_float(model()->parameters().transport_penalty_large) - to_float(model()->parameters().transport_penalty_small)) / 2;
}

void PurchasingManager::add_initial_demand_D_star(const Demand& demand_D_p) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    demand_D_ += demand_D_p;
    expected_costs_ += demand_D_p.get_value();
}

void PurchasingManager::subtract_initial_demand_D_star(const Demand& demand_D_p) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    demand_D_ -= demand_D_p;
    expected_costs_ -= demand_D_p.get_value();
}

void PurchasingManager::iterate_purchase() {
    debug::assertstep(this, IterationStep::PURCHASE);
    assert(!business_connections.empty());

    demand_D_ = Demand(0.0);
    expected_costs_ = FlowValue(0.0);
    optimized_value_ = 0.0;
    purchase_ = Demand(0.0);
    total_transport_penalty_ = FlowValue(0.0);

    demand_requests_D.clear();
    demand_requests_D.reserve(business_connections.size());
    lower_bounds.clear();
    lower_bounds.reserve(business_connections.size());
    purchasing_connections.clear();
    purchasing_connections.reserve(business_connections.size());
    upper_bounds.clear();
    upper_bounds.reserve(business_connections.size());
    xtol_abs.clear();
    xtol_abs.reserve(business_connections.size());

    const auto S_shortage = get_flow_deficit() * model()->delta_t() + storage->initial_content_S_star().get_quantity() - storage->content_S().get_quantity();

    desired_purchase_ = storage->desired_used_flow_U_tilde().get_quantity()  // desired used flow is either last or expected flow
                        + S_shortage
                              / (S_shortage > 0.0 ? storage->sector->parameters().target_storage_refill_time    // storage level low
                                                  : storage->sector->parameters().target_storage_withdraw_time  // storage level high
                              );

    if (round(desired_purchase_) <= 0.0) {
        for (auto& bc : business_connections) {
            bc->send_demand_request_D(Demand(0.0));
        }
        return;
    }

    FlowQuantity maximal_possible_purchase(0.0);
    for (auto& bc : business_connections) {
        if (bc->seller->communicated_parameters().possible_production_X_hat.get_quantity() <= 0.0) {
            bc->send_demand_request_D(Demand(0.0));
        } else {  // this supplier can deliver a non-zero amount
            // assumption, we cannot crowd out other purchasers given that our maximum offer price is n_max, calculate analytical approximation for maximal
            // deliverable amount of purchaser X_max(n_max) and consider boundary conditions
            const auto X_expected = expected_production(bc.get());
            const auto additional_X_expected = expected_additional_production(bc.get());
            auto X_max = to_float(calc_analytical_approximation_X_max(bc.get()));
            if constexpr (options::USE_MIN_PASSAGE_IN_EXPECTATION) {
                X_max *= bc->get_minimum_passage();
            }
            if constexpr (options::DEBUGGING) {
                const auto X_hat = to_float(bc->seller->communicated_parameters().possible_production_X_hat.get_quantity());
                assert(X_max <= X_hat);
                (void)X_hat;
            }
            const FloatType lower_limit = 0.0;
            const FloatType upper_limit = X_max - additional_X_expected;
            const auto D_r_max = round(FlowQuantity(upper_limit));
            if (D_r_max > 0.0) {
                const auto initial_value = std::min(upper_limit, std::max(lower_limit, X_expected - additional_X_expected));
                purchasing_connections.push_back(bc.get());
                lower_bounds.push_back(scaled_D_r(lower_limit, bc.get()));
                upper_bounds.push_back(scaled_D_r(upper_limit, bc.get()));
                xtol_abs.push_back(scaled_D_r(FlowQuantity::precision, bc.get()));
                demand_requests_D.push_back(scaled_D_r(initial_value, bc.get()));
                maximal_possible_purchase += D_r_max;
            } else {
                bc->send_demand_request_D(Demand(0.0));
            }
        }
    }

    if (purchasing_connections.empty()) {
        log::warning(this, "possible demand is zero (no supplier with possible production capacity > 0.0)");
        return;
    }
    assert(maximal_possible_purchase > 0.0);

    if (desired_purchase_ > maximal_possible_purchase) {
        desired_purchase_ = maximal_possible_purchase;
    }

    try {
        // experimental optimization setup: first use global optimizer DIRECT to get a reasonable result, polish the result with previous routine.
        // add auglag to support contraints with different global algorithms
        // define  lagrangian optimizer to pass (in)equality constraints to global algorithm which cannot use it directly:
        optimization::Optimization lagrangian_optimizer(static_cast<nlopt_algorithm>(model()->parameters().lagrangian_algorithm),
                                                        purchasing_connections.size());  // TODO keep and only recreate when resize is needed
        lagrangian_optimizer.add_equality_constraint(this, FlowValue::precision);
        lagrangian_optimizer.add_max_objective(this);
        lagrangian_optimizer.lower_bounds(lower_bounds);
        lagrangian_optimizer.upper_bounds(upper_bounds);
        lagrangian_optimizer.xtol(xtol_abs);
        lagrangian_optimizer.maxeval(model()->parameters().global_optimization_maxiter);
        lagrangian_optimizer.maxtime(model()->parameters().optimization_timeout);

        // define global optimizer
        optimization::Optimization pre_opt(static_cast<nlopt_algorithm>(model()->parameters().global_optimization_algorithm),
                                           purchasing_connections.size());  // TODO keep and only recreate when resize is needed

        pre_opt.xtol(xtol_abs);
        pre_opt.maxeval(model()->parameters().global_optimization_maxiter);
        pre_opt.maxtime(model()->parameters().optimization_timeout);
        // start combined global optimizer
        lagrangian_optimizer.set_local_algorithm(pre_opt.get_optimizer());
        const auto pre_res = lagrangian_optimizer.optimize(demand_requests_D);

        if (!pre_res && !pre_opt.xtol_reached()) {
            if (pre_opt.roundoff_limited()) {
                if constexpr (!IGNORE_ROUNDOFFLIMITED) {
                    if constexpr (options::DEBUGGING) {
                        debug_print_distribution(demand_requests_D);
                    }
                    model()->run()->event(EventType::OPTIMIZER_ROUNDOFF_LIMITED, storage->sector, storage->economic_agent);
                    if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                        throw log::error(this, "optimization is roundoff limited (for ", purchasing_connections.size(), " inputs)");
                    } else {
                        log::warning(this, "optimization is roundoff limited (for ", purchasing_connections.size(), " inputs)");
                    }
                }
            } else if (pre_opt.maxeval_reached()) {
                if constexpr (options::DEBUGGING) {
                    debug_print_distribution(demand_requests_D);
                }
                model()->run()->event(EventType::OPTIMIZER_MAXITER, storage->sector, storage->economic_agent);
                if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                    throw log::error(this, "optimization reached maximum iterations (for ", purchasing_connections.size(), " inputs)");
                } else {
                    log::warning(this, "optimization reached maximum iterations (for ", purchasing_connections.size(), " inputs)");
                }
            } else if (pre_opt.maxtime_reached()) {
                if constexpr (options::DEBUGGING) {
                    debug_print_distribution(demand_requests_D);
                }
                model()->run()->event(EventType::OPTIMIZER_TIMEOUT, storage->sector, storage->economic_agent);
                if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                    throw log::error(this, "optimization timed out (for ", purchasing_connections.size(), " inputs)");
                } else {
                    log::warning(this, "optimization timed out (for ", purchasing_connections.size(), " inputs)");
                }
            } else {
                log::warning(this, "optimization finished with ", pre_opt.last_result_description());
            }
        }

        optimization::Optimization opt(static_cast<nlopt_algorithm>(model()->parameters().optimization_algorithm),
                                       purchasing_connections.size());  // TODO keep and only recreate when resize is needed

        opt.add_equality_constraint(this, FlowQuantity::precision);
        opt.add_max_objective(this);

        opt.xtol(xtol_abs);
        opt.lower_bounds(lower_bounds);
        opt.upper_bounds(upper_bounds);
        opt.maxeval(model()->parameters().optimization_maxiter);
        opt.maxtime(model()->parameters().optimization_timeout);
        const auto res = opt.optimize(demand_requests_D);
        if (!res && !opt.xtol_reached()) {
            if (opt.roundoff_limited()) {
                if constexpr (!IGNORE_ROUNDOFFLIMITED) {
                    if constexpr (options::DEBUGGING) {
                        debug_print_distribution(demand_requests_D);
                    }
                    model()->run()->event(EventType::OPTIMIZER_ROUNDOFF_LIMITED, storage->sector, storage->economic_agent);
                    if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                        throw log::error(this, "optimization is roundoff limited (for ", purchasing_connections.size(), " inputs)");
                    } else {
                        log::warning(this, "optimization is roundoff limited (for ", purchasing_connections.size(), " inputs)");
                    }
                }
            } else if (opt.maxeval_reached()) {
                if constexpr (options::DEBUGGING) {
                    debug_print_distribution(demand_requests_D);
                }
                model()->run()->event(EventType::OPTIMIZER_MAXITER, storage->sector, storage->economic_agent);
                if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                    throw log::error(this, "optimization reached maximum iterations (for ", purchasing_connections.size(), " inputs)");
                } else {
                    log::warning(this, "optimization reached maximum iterations (for ", purchasing_connections.size(), " inputs)");
                }
            } else if (opt.maxtime_reached()) {
                if constexpr (options::DEBUGGING) {
                    debug_print_distribution(demand_requests_D);
                }
                model()->run()->event(EventType::OPTIMIZER_TIMEOUT, storage->sector, storage->economic_agent);
                if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                    throw log::error(this, "optimization timed out (for ", purchasing_connections.size(), " inputs)");
                } else {
                    log::warning(this, "optimization timed out (for ", purchasing_connections.size(), " inputs)");
                }
            } else {
                log::warning(this, "optimization finished with ", opt.last_result_description());
            }
        }
        optimized_value_ = unscaled_objective(opt.optimized_value());

    } catch (const optimization::failure& ex) {
        if constexpr (options::DEBUGGING) {
            debug_print_distribution(demand_requests_D);
        }
        throw log::error(this, "optimization failed, ", ex.what(), " (for ", purchasing_connections.size(), " inputs)");
    }

    FloatType costs = 0.0;
    FloatType use = 0.0;
    // distribute demand requests
    for (std::size_t r = 0; r < purchasing_connections.size(); ++r) {
        const auto D_r = unscaled_D_r(demand_requests_D[r], purchasing_connections[r]);
        Demand demand_request_D = Demand(FlowQuantity(D_r), FlowValue(D_r));
        assert(!std::isnan(n_r(D_r, purchasing_connections[r])));
        demand_request_D.set_price(round(Price(n_r(D_r, purchasing_connections[r]))));

        if constexpr (options::OPTIMIZATION_WARNINGS) {
            if (round(demand_request_D.get_quantity()) > round(FlowQuantity(unscaled_D_r(upper_bounds[r], purchasing_connections[r])))) {
                log::warning(this, purchasing_connections[r]->name(), ": upper limit overshot in optimization D_r: ", demand_request_D.get_quantity(),
                             " D_max: ", upper_bounds[r], " D_star: ", purchasing_connections[r]->initial_flow_Z_star().get_quantity());
            }
            if (round(demand_request_D.get_quantity()) < round(FlowQuantity(unscaled_D_r(lower_bounds[r], purchasing_connections[r])))) {
                log::warning(this, purchasing_connections[r]->name(), ": lower limit overshot in optimization D_r: ", demand_request_D.get_quantity(),
                             " D_min: ", lower_bounds[r], " D_star: ", purchasing_connections[r]->initial_flow_Z_star().get_quantity());
            }
        }
        purchasing_connections[r]->send_demand_request_D(round(demand_request_D));

        if constexpr (options::DEBUGGING) {
            use += D_r;
        }
        demand_D_ += round(demand_request_D);
        costs += n_r(D_r, purchasing_connections[r]) * D_r + transport_penalty(D_r, purchasing_connections[r]);
        total_transport_penalty_ += FlowValue(transport_penalty(D_r, purchasing_connections[r]));
    }
    expected_costs_ = FlowValue(costs);
    if constexpr (options::DEBUGGING) {
        assert(FlowQuantity(use) >= 0.0);
    }
    purchase_ = demand_D_;
}

FlowQuantity PurchasingManager::calc_analytical_approximation_X_max(const BusinessConnection* bc) {
    return bc->seller->communicated_parameters().possible_production_X_hat.get_quantity();
}

void PurchasingManager::debug_print_details() const {
    if constexpr (options::DEBUGGING) {
        log::info(this, business_connections.size(), " inputs:  I_star= ", storage->initial_input_flow_I_star().get_quantity());
        for (const auto& bc : business_connections) {
            log::info(this, "    ", bc->name(), ":  Z_star= ", std::setw(11), bc->initial_flow_Z_star().get_quantity(), "  X_star= ", std::setw(11),
                      bc->seller->firm->initial_production_X_star().get_quantity());
        }
    }
}

template<typename T1, typename T2>
static void print_row(T1 a, T2 b) {
    std::cout << "      " << std::setw(14) << a << " = " << std::setw(14) << b << '\n';
}

template<typename T1, typename T2, typename T3>
static void print_row(T1 a, T2 b, T3 c) {
    std::cout << "      " << std::setw(14) << a << " = " << std::setw(14) << b << " (" << c << ")\n";
}

void PurchasingManager::debug_print_distribution(const std::vector<double>& demand_requests_D) const {
    if constexpr (options::DEBUGGING) {
#pragma omp critical(output)
        {
            std::cout << model()->run()->timeinfo() << ", " << name() << ": demand distribution for " << purchasing_connections.size() << " inputs :\n";
            FloatType purchasing_quantity = 0.0;
            FloatType purchasing_value = 0.0;
            FloatType initial_sum = 0.0;
            std::vector<FloatType> last_demand_requests(purchasing_connections.size());
            std::vector<FloatType> grad(purchasing_connections.size());

            const auto obj = max_objective(&demand_requests_D[0], &grad[0]);
            FloatType total_upper_bound = 0.0;
            FloatType T_penalty = 0.0;
            for (std::size_t r = 0; r < purchasing_connections.size(); ++r) {
                const auto bc = purchasing_connections[r];
                const auto D_r = unscaled_D_r(demand_requests_D[r], bc);
                const auto lower_bound_D_r = unscaled_D_r(lower_bounds[r], bc);
                const auto upper_bound_D_r = unscaled_D_r(upper_bounds[r], bc);
                const auto X = to_float(bc->seller->communicated_parameters().production_X.get_quantity());
                const auto X_hat = to_float(bc->seller->communicated_parameters().possible_production_X_hat.get_quantity());
                const auto X_star = to_float(bc->seller->firm->initial_production_X_star().get_quantity());

                total_upper_bound += X_hat;
                const auto n_bar = to_float(bc->seller->communicated_parameters().offer_price_n_bar);
                const auto n_r_l = n_r(D_r, bc);
                const auto n_r_tc_l = transport_penalty(D_r, bc);
                T_penalty += n_r_tc_l;
                last_demand_requests[r] = to_float(bc->last_demand_request_D(this).get_quantity());

                if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                    std::cout << "      " << bc->name() << " :\n";
                    print_row("X", FlowQuantity(X));
                    print_row("X_star", FlowQuantity(X_star));
                    print_row("X_hat", FlowQuantity(X_hat));
                    print_row("n_bar", FlowQuantity(n_bar));
                    print_row("n_r(D_r)", FlowQuantity(n_r_l));
                    print_row("penalty_t", FlowQuantity(n_r_tc_l), scaled_objective(n_r_tc_l));
                    print_row("Z_last", bc->last_shipment_Z().get_quantity(), scaled_D_r(to_float(bc->last_shipment_Z().get_quantity()), bc));
                    print_row("D_star", bc->initial_flow_Z_star().get_quantity());
                    print_row("out_share", bc->initial_flow_Z_star() / bc->seller->firm->initial_production_X_star());
                    print_row("grad", grad[r]);
                    print_row("D_lower", FlowQuantity(lower_bound_D_r), scaled_D_r(lower_bound_D_r, bc));
                    print_row("D_r", FlowQuantity(D_r), scaled_D_r(D_r, bc));
                    print_row("D_upper", FlowQuantity(upper_bound_D_r), scaled_D_r(upper_bound_D_r, bc));
                    std::cout << '\n';
                }
                purchasing_quantity += D_r;
                purchasing_value += n_r_l * D_r;
                initial_sum += to_float(bc->initial_flow_Z_star().get_quantity());
            }
            const auto use = purchasing_quantity;
            const auto transport_flow_deficit = get_flow_deficit();
            FloatType S_shortage = to_float(transport_flow_deficit)
                                   + to_float((storage->initial_content_S_star().get_quantity() - storage->content_S().get_quantity()) / model()->delta_t());
            FloatType n_r_bar = (purchasing_quantity > 0.0) ? purchasing_value / purchasing_quantity : 0.0;
            std::cout << "      Storage :\n";
            print_row("av. n_r", Price(n_r_bar));
            print_row("S", storage->content_S().get_quantity());
            print_row("S_star", storage->initial_content_S_star().get_quantity());
            print_row("S_shortage", Quantity(S_shortage));
            print_row("T_def", transport_flow_deficit, scaled_use(to_float(transport_flow_deficit)));
            print_row("T_penalty", FlowValue(T_penalty), scaled_objective(T_penalty));
            std::cout << "\n";

            std::cout << "      Sums :\n";
            print_row("X_hat", FlowQuantity(total_upper_bound));
            print_row("D_orig", demand_D_.get_quantity());
            print_row("purchase", FlowQuantity(purchasing_quantity), scaled_use(use));
            print_row("D_star", FlowQuantity(initial_sum));
            print_row("U_i", FlowQuantity(use), scaled_use(use));
            print_row("obj", FlowValue(unscaled_objective(obj)), obj);
            std::cout << "\n";
        }
    }
}

}  // namespace acclimate
