// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/SalesManager.h"

#include "ModelRun.h"
#include "acclimate.h"
#include "model/BusinessConnection.h"
#include "model/CapacityManager.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/PurchasingManager.h"
#include "model/Sector.h"
#include "model/Storage.h"

namespace acclimate {

SalesManager::SalesManager(Firm* firm_) : firm(firm_) {}

void SalesManager::add_demand_request(const Demand& demand_request) {
    debug::assertstep(this, IterationStep::PURCHASE);
    firm->sector->add_demand_request(demand_request);
    sum_demand_requests_lock_.call([&]() { sum_demand_requests_ += demand_request; });
}

void SalesManager::add_baseline_demand(const Demand& demand_request) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    sum_demand_requests_ += demand_request;
}

void SalesManager::subtract_baseline_demand(const Demand& demand_request) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    sum_demand_requests_ -= demand_request;
}

auto SalesManager::get_transport_flow() const -> Flow {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return std::accumulate(std::begin(business_connections), std::end(business_connections), Flow(0.0),
                           [](const Flow& f, const auto& bc) { return f + bc->get_transport_flow(); });
}

auto SalesManager::remove_business_connection(BusinessConnection* business_connection) -> bool {
    auto it = std::find_if(std::begin(business_connections), std::end(business_connections),
                           [business_connection](const auto& bc) { return bc.get() == business_connection; });
    if (it == std::end(business_connections)) {
        throw log::error(this, "Business connection ", business_connection->name(), " not found");
    }
    business_connections.erase(it);
    if constexpr (Options::DEBUGGING) {
        if (business_connections.empty()) {
            debug::assertstep(this, IterationStep::INITIALIZATION);
            return true;
        }
    }
    return false;
}

SalesManager::~SalesManager() {
    for (auto& bc : business_connections) {
        bc->seller.invalidate();
    }
}

auto SalesManager::model() -> Model* { return firm->model(); }
auto SalesManager::model() const -> const Model* { return firm->model(); }

auto SalesManager::name() const -> std::string { return firm->name(); }

auto SalesManager::sum_demand_requests() const -> const Demand& {
    debug::assertstepnot(this, IterationStep::PURCHASE);
    return sum_demand_requests_;
}

auto SalesManager::calc_production() -> Flow {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    assert(!business_connections.empty());
    sum_demand_requests_ = round(sum_demand_requests_);

    // sort all incoming connections by price (descending), then by quantity (descending)
    std::sort(
        business_connections.begin(), business_connections.end(),
        [](const std::shared_ptr<BusinessConnection>& business_connection_1, const std::shared_ptr<BusinessConnection>& business_connection_2) {
            if (business_connection_1->last_demand_request().get_quantity() <= 0.0 && business_connection_2->last_demand_request().get_quantity() > 0.0) {
                // we want to store empty demand requests at the end of the business connections
                return false;
            }
            if (business_connection_1->last_demand_request().get_quantity() > 0.0 && business_connection_2->last_demand_request().get_quantity() <= 0.0) {
                return true;
            }
            if (business_connection_1->last_demand_request().get_quantity() <= 0.0 && business_connection_2->last_demand_request().get_quantity() <= 0.0) {
                // both are empty --> ordering does not matter
                return false;
            }
            if (business_connection_1->last_demand_request().get_price() > business_connection_2->last_demand_request().get_price()) {
                return true;
            }
            if (business_connection_1->last_demand_request().get_price() < business_connection_2->last_demand_request().get_price()) {
                return false;
            }
            // none of them is empty, but prices are equal
            return (business_connection_1->last_demand_request().get_quantity() > business_connection_2->last_demand_request().get_quantity());
        });

    Flow possible_production = firm->capacity_manager->possible_production();
    if (estimated_possible_production_.get_quantity() > 0.0) {
        // price (unit_production_costs) has been calculated already in estimation step
        possible_production.set_price(estimated_possible_production_.get_price());
    }
    std::tie(communicated_parameters_.production, communicated_parameters_.offer_price) = calc_supply_distribution_scenario(possible_production);
    communicated_parameters_.possible_production = possible_production;

    return communicated_parameters_.production;
}

void SalesManager::distribute() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    assert(!business_connections.empty());
    // push all flows
    if (communicated_parameters_.production.get_quantity() <= 0.0) {  // no production
        for (auto& not_served_bc : business_connections) {
            not_served_bc->push_flow(Flow(0.0));
        }
    } else {                            // non-zero production to distribute
        unsigned int pushed_flows = 0;  // only used when debugging
        assert(!isnan(supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization));
        Price cheapest_price_range_half_width(0.0);
        if (model()->parameters().cheapest_price_range_generic_size) {
            cheapest_price_range_half_width = firm->sector->parameters().price_increase_production_extension / 2
                                              * (firm->capacity_manager->possible_overcapacity_ratio_beta - 1)
                                              * (firm->capacity_manager->possible_overcapacity_ratio_beta - 1)
                                              / firm->capacity_manager->possible_overcapacity_ratio_beta;  // = maximal penalty per unit
        } else {
            cheapest_price_range_half_width = model()->parameters().cheapest_price_range_width / 2;
        }
        auto begin_cheapest_price_range = business_connections.begin();
        auto end_cheapest_price_range = business_connections.begin();
        auto demand_cheapest_price_range = FlowQuantity(0.0);
        auto demand_value_cheapest_price_range = FlowValue(0.0);
        Flow production_without_cheapest_price_range = Flow(0.0);
        for (auto served_bc = business_connections.begin(); served_bc != business_connections.end(); ++served_bc) {
            if ((*served_bc)->last_demand_request().get_quantity() > 0.0) {
                if (round((*served_bc)->last_demand_request().get_price() - supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization)
                    >= cheapest_price_range_half_width) {  // price higher than in cheapest price range
                    assert((*served_bc)->last_demand_request().get_quantity() <= communicated_parameters_.production.get_quantity());
                    (*served_bc)->push_flow(round((*served_bc)->last_demand_request()));
                    production_without_cheapest_price_range += round((*served_bc)->last_demand_request());
                    if constexpr (Options::DEBUGGING) {
                        ++pushed_flows;
                    }
                    begin_cheapest_price_range = served_bc + 1;
                } else if (abs(round((*served_bc)->last_demand_request().get_price()
                                     - supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization))
                           < cheapest_price_range_half_width) {  // price in cheapest price range
                    demand_cheapest_price_range += (*served_bc)->last_demand_request().get_quantity();
                    demand_value_cheapest_price_range += (*served_bc)->last_demand_request().get_value();
                    // flow is pushed later
                    end_cheapest_price_range = served_bc + 1;
                } else {  // price lower than in cheapest price range
                    (*served_bc)->push_flow(Flow(0.0));
                    if constexpr (Options::DEBUGGING) {
                        ++pushed_flows;
                    }
                }
            } else {  // demand request is zero
                (*served_bc)->push_flow(Flow(0.0));
                if constexpr (Options::DEBUGGING) {
                    ++pushed_flows;
                }
            }
        }
        if (begin_cheapest_price_range < end_cheapest_price_range) {
            const FlowQuantity production_cheapest_price_range =
                round(communicated_parameters_.production.get_quantity() - production_without_cheapest_price_range.get_quantity());
            if (demand_cheapest_price_range > production_cheapest_price_range) {  // not all demands in cheapest price range can be fulfilled
                const Price max_price_cheapest_price_range =
                    supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization
                    + cheapest_price_range_half_width;  // (*begin_cheapest_price_range)->last_demand_request().get_price();
                const Price min_price_cheapest_price_range =
                    supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization
                    - cheapest_price_range_half_width;  // (*(end_cheapest_price_range - 1))->last_demand_request().get_price();
                const Price average_price_cheapest_price_range = demand_value_cheapest_price_range / demand_cheapest_price_range;
                const Price price_shift = std::max(
                    (max_price_cheapest_price_range * production_cheapest_price_range - average_price_cheapest_price_range * demand_cheapest_price_range)
                        / (demand_cheapest_price_range - production_cheapest_price_range),
                    Price(0.0) - min_price_cheapest_price_range);
                const Price price_as_calculated_in_distribution_scenario =
                    (communicated_parameters_.production.get_value() - production_without_cheapest_price_range.get_value()) / production_cheapest_price_range;

                total_revenue_ = production_without_cheapest_price_range.get_value();
                // serve demand requests in equal distribution range
                for (auto served_bc = begin_cheapest_price_range; served_bc != end_cheapest_price_range; ++served_bc) {
                    Flow const flow_Z =
                        Flow(round(FlowQuantity(
                                 to_float(production_cheapest_price_range)
                                 * to_float((*served_bc)->last_demand_request().get_quantity() * price_shift + (*served_bc)->last_demand_request().get_value())
                                 / to_float(demand_cheapest_price_range * price_shift + demand_value_cheapest_price_range))),
                             model()->parameters().cheapest_price_range_preserve_seller_price ? price_as_calculated_in_distribution_scenario
                                                                                              : (*served_bc)->last_demand_request().get_price());
                    assert(flow_Z.get_quantity() <= (*served_bc)->last_demand_request().get_quantity());
                    (*served_bc)->push_flow(flow_Z);
                    total_revenue_ += flow_Z.get_value();
                    if constexpr (Options::DEBUGGING) {
                        ++pushed_flows;
                    }
                }
            } else {  // all demands in cheapest price range can be fulfilled
                total_revenue_ = communicated_parameters_.production.get_value();
                for (auto served_bc = begin_cheapest_price_range; served_bc != end_cheapest_price_range; ++served_bc) {
                    assert((*served_bc)->last_demand_request().get_quantity() <= communicated_parameters_.production.get_quantity());
                    (*served_bc)->push_flow(round((*served_bc)->last_demand_request()));
                    if constexpr (Options::DEBUGGING) {
                        ++pushed_flows;
                    }
                }
            }
        } else {
            total_revenue_ = communicated_parameters_.production.get_value();
        }
        if constexpr (Options::DEBUGGING) {
            if (pushed_flows < business_connections.size()) {
                log::info(this, pushed_flows);
                log::info(this, business_connections.size());
            }
            assert(pushed_flows == business_connections.size());
        }
    }
}

auto SalesManager::calc_supply_distribution_scenario(const Flow& possible_production) -> std::tuple<Flow, Price> {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);

    // find first (in order) connection with zero demand
    auto first_zero_connection = std::find_if(std::begin(business_connections), std::end(business_connections),
                                              [](const auto& business_connection) { return business_connection->last_demand_request().get_quantity() <= 0.0; });

    supply_distribution_scenario.connection_not_served_completely = first_zero_connection;
    total_production_costs_ = FlowValue(0.0);
    Price const minimal_production_price = round(possible_production.get_price());
    Price const minimal_offer_price = round(minimal_production_price + get_baseline_markup() / (1 - tax_));

    if (possible_production.get_quantity() <= 0.0) {
        // no production due to supply shortage or forcing == 0

        if (firm->forcing() <= 0.0) {
            log::warning(this, "no production due to total forcing");
        } else {
            model()->run()->event(EventType::NO_PRODUCTION_SUPPLY_SHORTAGE, firm);
        }
        supply_distribution_scenario.connection_not_served_completely = business_connections.begin();
        supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
        supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization = Price::quiet_NaN();
        return std::make_tuple(Flow(0.0), Price::quiet_NaN());
    }
    if (first_zero_connection == business_connections.begin()  // no production due to demand quantity shortage
        || round((*business_connections.begin())->last_demand_request().get_price()) < minimal_production_price  // no production due to demand value shortage
    ) {
        // no production due to demand shortage

        if (first_zero_connection == business_connections.begin()) {
            model()->run()->event(EventType::NO_PRODUCTION_DEMAND_QUANTITY_SHORTAGE, firm);
        } else {
            model()->run()->event(EventType::NO_PRODUCTION_DEMAND_VALUE_SHORTAGE, firm);
        }
        supply_distribution_scenario.connection_not_served_completely = business_connections.begin();
        supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
        // communicate least price we would produce for:
        supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization = Price::quiet_NaN();
        assert(!isnan(minimal_production_price));
        return std::make_tuple(Flow(0.0), Price::quiet_NaN());
    }
    // non-zero production

    Flow production = Flow(0.0);

    // cycle over non-zero connections
    for (auto business_connection = business_connections.begin(); business_connection != first_zero_connection; ++business_connection) {
        // check that connection can be served at all
        if (round((production + (*business_connection)->last_demand_request())).get_quantity() > possible_production.get_quantity()) {
            supply_distribution_scenario.connection_not_served_completely = business_connection;
            break;
        }
        // check that price of demand request is high enough
        Price const maximal_marginal_production_costs =
            calc_marginal_production_costs((round(production + (*business_connection)->last_demand_request())).get_quantity(), minimal_production_price);
        if (isnan(maximal_marginal_production_costs)  // infinite costs
            || round((*business_connection)->last_demand_request().get_price()) < round(maximal_marginal_production_costs)) {
            supply_distribution_scenario.connection_not_served_completely = business_connection;
            break;
        }
        production += (*business_connection)->last_demand_request();
    }
    production = round(production);
    assert(production.get_quantity() <= possible_production.get_quantity());

    if (supply_distribution_scenario.connection_not_served_completely == first_zero_connection) {
        // all demand requests have been served completely to the price they offered
        auto cheapest_served_non_zero_connection = first_zero_connection - 1;      // note: first_zero_connection == business_connections.begin() is excluded
        assert(production.get_quantity() <= sum_demand_requests_.get_quantity());  // to see if we make rounding errors
        assert(production.get_quantity() >= sum_demand_requests_.get_quantity());  // to see if we make rounding errors
        total_production_costs_ = calc_total_production_costs(production, minimal_production_price);
        // check for supply limitation offer price is reduced to attract more demand
        if (round(firm->capacity_manager->desired_production().get_quantity())
                < firm->forced_baseline_production_quantity()  // not in production extension and ...
            && round(firm->capacity_manager->desired_production().get_quantity())
                   < round(possible_production.get_quantity())  // ... and demand quantity limited
        ) {                                                     // note: round needed, otherwise the loop is entered also in equilibrium
            // demand quantity limitation
            assert(!isnan(minimal_production_price));
            // linear price reduction (is overwritten when considering expectation)
            const Price offer_price =
                round(std::max(production.get_price()
                                   * (1
                                      + firm->sector->parameters().supply_elasticity
                                            * ((production - (firm->forced_baseline_production())) / (firm->forced_baseline_production()))),
                               minimal_offer_price));
            supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization =
                (*cheapest_served_non_zero_connection)->last_demand_request().get_price();
            return std::make_tuple(production, offer_price);
        }
        // no demand quantity limitation sum_D >= lambda * X_star
        const Price offer_price = round(production.get_price());
        supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization =
            round((*cheapest_served_non_zero_connection)->last_demand_request().get_price());
        return std::make_tuple(production, offer_price);
    }
    // not all demand requests have been served completely
    if (round(goal_fkt_marginal_costs_minus_price(production.get_quantity(), minimal_production_price,
                                                  (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request().get_price()))
        < 0.0) {
        // we will send a non-zero amount to connection_not_served_completely

        if (round(goal_fkt_marginal_costs_minus_price(possible_production.get_quantity(), minimal_production_price,
                                                      (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request().get_price()))
            < 0.0)
        // price is high enough, we would send as much as we can to the purchaser
        {
            supply_distribution_scenario.flow_not_served_completely =
                round(Flow(possible_production.get_quantity() - production.get_quantity(),
                           (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request().get_price()));
            assert(supply_distribution_scenario.flow_not_served_completely.get_quantity() >= 0.0);
            supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization =
                (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request().get_price();
        } else {  // not supply limited
            // price is not high enough to fulfill complete demand request, we have to find the optimal production ratio
            FlowQuantity const total_production_quantity = analytic_solution_in_production_extension(
                minimal_production_price, (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request().get_price());
            assert(total_production_quantity >= production.get_quantity());

            if (total_production_quantity > production.get_quantity()) {
                supply_distribution_scenario.flow_not_served_completely =
                    round(Flow(total_production_quantity - production.get_quantity(),
                               (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request().get_price()));
                assert(supply_distribution_scenario.flow_not_served_completely.get_quantity() >= 0.0);
                supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization =
                    (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request().get_price();
            } else {  // zero flow is sent
                supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
                if (supply_distribution_scenario.connection_not_served_completely == business_connections.begin()) {
                    // we have actually no production at all
                    // offer_price and cutoff_price will be set below
                    assert(production.get_quantity() <= 0.0);
                } else {
                    auto cheapest_served_connection = supply_distribution_scenario.connection_not_served_completely - 1;
                    // note: connection_not_served_completely != business_connections.begin(), otherwise: no production
                    supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization =
                        (*cheapest_served_connection)->last_demand_request().get_price();
                }
            }
        }
        production += supply_distribution_scenario.flow_not_served_completely;
        production = round(production);
        assert(production.get_quantity() <= possible_production.get_quantity());
    } else {  // we will send a zero-amount to connection_not_served_completely
        supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
        if (supply_distribution_scenario.connection_not_served_completely != business_connections.begin()) {
            // we have a non-zero production
            auto cheapest_served_connection = supply_distribution_scenario.connection_not_served_completely - 1;
            // note: connection_not_served_completely != business_connections.begin(), otherwise: no production
            supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization = (*cheapest_served_connection)->last_demand_request().get_price();
        }
    }
    total_production_costs_ = calc_total_production_costs(production, minimal_production_price);
    if (production.get_quantity() > 0.0) {
        // we have a non-zero production
        const Price offer_price = round(production.get_price());
        return std::make_tuple(production, offer_price);
    }
    // no production
    model()->run()->event(EventType::NO_PRODUCTION_HIGH_COSTS, firm);
    log::warning(this, "no production because of high costs / too low priced demand requests");
    supply_distribution_scenario.connection_not_served_completely = business_connections.begin();
    supply_distribution_scenario.flow_not_served_completely = Flow(0.0);

    // communicate least price we would produce for:
    supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization = Price::quiet_NaN();
    const Price& offer_price = minimal_production_price;
    return std::make_tuple(production, offer_price);
}

auto SalesManager::calc_expected_supply_distribution_scenario(const Flow& possible_production) -> std::tuple<Flow, Price> {
    debug::assertstep(this, IterationStep::EXPECTATION);
    // find first (in order) connection with zero demand
    auto first_zero_connection = std::find_if(std::begin(business_connections), std::end(business_connections),
                                              [](const auto& business_connection) { return business_connection->last_demand_request().get_quantity() <= 0.0; });

    supply_distribution_scenario.connection_not_served_completely = first_zero_connection;
    Price const minimal_production_price = round(possible_production.get_price());
    Price const minimal_offer_price = round(minimal_production_price + get_baseline_markup() / (1 - tax_));

    if (possible_production.get_quantity() <= 0.0) {
        // no production due to supply shortage or forcing <= 0

        if (firm->forcing() <= 0.0) {
            log::warning(this, "no expected production due to total forcing");
        } else {
            model()->run()->event(EventType::NO_EXP_PRODUCTION_SUPPLY_SHORTAGE, firm);
        }
        supply_distribution_scenario.connection_not_served_completely = business_connections.begin();
        supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
        supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization = Price::quiet_NaN();
        return std::make_tuple(Flow(0.0), Price::quiet_NaN());
    }
    if (first_zero_connection == business_connections.begin()  // no production due to demand quantity shortage
        || round((*business_connections.begin())->last_demand_request().get_price()) < minimal_production_price) {
        // no production due to demand shortage

        if (first_zero_connection == business_connections.begin()) {
            model()->run()->event(EventType::NO_EXP_PRODUCTION_DEMAND_QUANTITY_SHORTAGE, firm);
        } else {
            model()->run()->event(EventType::NO_EXP_PRODUCTION_DEMAND_VALUE_SHORTAGE, firm);
        }
        supply_distribution_scenario.connection_not_served_completely = business_connections.begin();
        supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
        // communicate least price we would produce for:
        supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization = Price::quiet_NaN();
        assert(!isnan(minimal_production_price));
        const Price& offer_price = minimal_production_price;
        Flow const expected_production = Flow(std::min(firm->forced_baseline_production().get_quantity(), possible_production.get_quantity()), offer_price);
        return std::make_tuple(expected_production, offer_price);
    }
    // non-zero expected production

    Flow expected_production = Flow(0.0);

    // cycle over non-zero connections
    for (auto business_connection = business_connections.begin(); business_connection != first_zero_connection; ++business_connection) {
        // check that connection can be served under quantitative aspects
        if (round((expected_production + (*business_connection)->last_demand_request())).get_quantity() > possible_production.get_quantity()) {
            supply_distribution_scenario.connection_not_served_completely = business_connection;
            break;
        }
        // check that price of demand request is high enough
        auto maximal_marginal_production_costs = Price(0.0);

        if (!model()->parameters().respect_markup_in_production_extension                        // we always want to attract more demand
            || expected_production.get_quantity() < firm->forced_baseline_production_quantity()  // ... production is below lambda * X_star
        ) {
            maximal_marginal_production_costs = calc_marginal_production_costs(
                (round(expected_production + (*business_connection)->last_demand_request())).get_quantity(), minimal_production_price);
        } else {
            maximal_marginal_production_costs = calc_marginal_production_costs(
                (round(expected_production + (*business_connection)->last_demand_request())).get_quantity(), minimal_offer_price);
        }
        if (isnan(maximal_marginal_production_costs)  // infinite costs
            || round((*business_connection)->last_demand_request().get_price()) < round(maximal_marginal_production_costs)) {
            supply_distribution_scenario.connection_not_served_completely = business_connection;
            break;
        }
        expected_production += (*business_connection)->last_demand_request();
    }
    expected_production = round(expected_production);
    assert(expected_production.get_quantity() <= possible_production.get_quantity());

    if (supply_distribution_scenario.connection_not_served_completely == first_zero_connection) {
        // all demand requests would be served completely to the price they offered
        auto cheapest_served_non_zero_connection = first_zero_connection - 1;  // note: first_zero_connection == business_connections.begin() is excluded
        // Expectations: the demand curves have to be extended
        // check that we reach the regime where X > sum_of_demand_requests

        constexpr bool respect_markup_in_extension_of_revenue_curve = true;

        if ((model()->parameters().always_extend_expected_demand_curve                            // we always want to attract more demand
             || expected_production.get_quantity() < firm->forced_baseline_production_quantity()  // ... production is below lambda * X_star
             )
            && !model()->parameters().naive_expectations) {
            if (round(goal_fkt_marginal_costs_minus_marginal_revenue(
                    expected_production.get_quantity(), respect_markup_in_extension_of_revenue_curve ? minimal_offer_price : minimal_production_price,
                    (*cheapest_served_non_zero_connection)->last_demand_request().get_price()))
                < 0.0) {
                // additional_expected_flow is non-zero

                Flow additional_expected_flow = Flow(0.0);
                FlowQuantity const maximal_additional_production_quantity =
                    round(std::min(possible_production.get_quantity(), firm->forced_maximal_production_quantity()) - expected_production.get_quantity());
                if (round(goal_fkt_marginal_costs_minus_marginal_revenue(
                        round(expected_production.get_quantity() + maximal_additional_production_quantity),
                        respect_markup_in_extension_of_revenue_curve ? minimal_offer_price : minimal_production_price,
                        (*cheapest_served_non_zero_connection)->last_demand_request().get_price()))
                    < 0.0) {
                    // price is high enough, we would provide as much as we can to the purchasers
                    additional_expected_flow =
                        Flow(maximal_additional_production_quantity,
                             calc_additional_revenue_expectation(round(expected_production.get_quantity() + maximal_additional_production_quantity),
                                                                 (*cheapest_served_non_zero_connection)->last_demand_request().get_price()));
                } else {
                    // price is not high enough to fulfill complete demand request, we have to find the optimal production ratio
                    auto precision_root_finding = Price(Price::precision);
                    additional_expected_flow =
                        search_root_bisec_expectation(FlowQuantity(0.0), maximal_additional_production_quantity, expected_production.get_quantity(),
                                                      respect_markup_in_extension_of_revenue_curve ? minimal_offer_price : minimal_production_price,
                                                      (*cheapest_served_non_zero_connection)->last_demand_request().get_price(),
                                                      precision_root_finding);  // note: true: initial markup respected
                    assert(round(additional_expected_flow).get_quantity() >= 0.0);
                }
                expected_production = round(expected_production + additional_expected_flow);
            }
        }

        // n_bar respects markup
        const Price offer_price = round(std::max(expected_production.get_price(),
                                                 calc_total_production_costs(expected_production, minimal_offer_price) / expected_production.get_quantity()));
        // n_co respects production costs
        assert(expected_production.get_quantity() <= possible_production.get_quantity());
        return std::make_tuple(expected_production, offer_price);
    }
    // not all demand requests would be served completely
    auto minimal_price = Price(0.0);
    if (!model()->parameters().respect_markup_in_production_extension                        // we always want to attract more demand
        || expected_production.get_quantity() < firm->forced_baseline_production_quantity()  // ... production is below lambda * X_star
    ) {
        minimal_price = minimal_production_price;
    } else {
        minimal_price = minimal_offer_price;
    }
    if (round(goal_fkt_marginal_costs_minus_price(expected_production.get_quantity(), minimal_price,
                                                  (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request().get_price()))
        < 0.0) {
        // price of connection_not_served_completely is high enough, we would send non-zero amount

        if (round(goal_fkt_marginal_costs_minus_price(possible_production.get_quantity(), minimal_price,
                                                      (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request().get_price()))
            < 0.0)
        // price is high enough, we would send as much as we can to the purchaser
        {
            supply_distribution_scenario.flow_not_served_completely =
                round(Flow(possible_production.get_quantity() - expected_production.get_quantity(),
                           (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request().get_price()));
            assert(supply_distribution_scenario.flow_not_served_completely.get_quantity() >= 0.0);
        } else {
            // price is not high enough to fulfill complete
            // demand request, we have to find the optimal
            // production ratio
            FlowQuantity const total_production_quantity = analytic_solution_in_production_extension(
                minimal_price, (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request().get_price());
            assert(total_production_quantity >= expected_production.get_quantity());

            if (total_production_quantity > expected_production.get_quantity()) {
                // production_not_served_completely would get a non-zero amount
                supply_distribution_scenario.flow_not_served_completely =
                    Flow(total_production_quantity - expected_production.get_quantity(),
                         (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request().get_price());
            } else {  // zero flow would be sent to production_not_served_completely
                supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
            }
        }
        expected_production += supply_distribution_scenario.flow_not_served_completely;
        expected_production = round(expected_production);
        assert(expected_production.get_quantity() <= possible_production.get_quantity());
    } else {  // we would send a zero-amount to connection_not_served_completely
        supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
    }
    if (expected_production.get_quantity() > 0.0) {
        // we have a non-zero production
        // n_bar respects markup and nco was set above
        const Price offer_price = round(std::max(expected_production.get_price(),
                                                 calc_total_production_costs(expected_production, minimal_offer_price) / expected_production.get_quantity()));
        return std::make_tuple(expected_production, offer_price);
    }
    // zero production
    model()->run()->event(EventType::NO_EXP_PRODUCTION_HIGH_COSTS, firm);
    // communicate least price we would produce for:
    const Price offer_price = minimal_offer_price;
    return std::make_tuple(expected_production, offer_price);
}

void SalesManager::iterate_expectation() {
    debug::assertstep(this, IterationStep::EXPECTATION);
    estimated_possible_production_ = firm->capacity_manager->estimate_possible_production();
    if (estimated_possible_production_.get_quantity() > 0.0) {
        estimated_possible_production_.set_price(estimated_possible_production_.get_price()
                                                 / (1 - tax_));  // tax is applied here (1-tax_) because tax is on revenue!
    }
    std::tie(communicated_parameters_.expected_production, communicated_parameters_.offer_price) =
        calc_expected_supply_distribution_scenario(estimated_possible_production_);
    sum_demand_requests_ = Flow(0.0);
}

auto SalesManager::get_baseline_unit_variable_production_costs() const -> Price {
    return std::max(Price(0.0), Price(1.0) - (baseline_unit_commodity_costs_ + get_baseline_markup()));
}

auto SalesManager::get_baseline_markup() const -> Price {
    return std::min(Price(1.0) - baseline_unit_commodity_costs_, firm->sector->parameters().baseline_markup);
}

void SalesManager::initialize() {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    baseline_unit_commodity_costs_ = Price(0.0);
    for (auto& input_storage : firm->input_storages) {
        baseline_unit_commodity_costs_ += Price(1.0) * input_storage->technology_coefficient();
    }
    assert(baseline_unit_commodity_costs_ > 0.0);
    assert(baseline_unit_commodity_costs_ <= 1);
    estimated_possible_production_ = firm->maximal_production();
    // actually only needed if we calculate possible_production in estimation step (but we already need it in production step)
    estimated_possible_production_.set_price(baseline_unit_commodity_costs_ + get_baseline_unit_variable_production_costs());
}

auto SalesManager::calc_total_production_costs(const Flow& production, const Price& unit_production_costs) const -> FlowValue {
    const FlowQuantity& production_quantity = production.get_quantity();
    assert(production_quantity >= 0.0);
    assert(!isnan(unit_production_costs));
    if (production_quantity <= firm->forced_baseline_production_quantity()) {  // not in production extension
        return production_quantity * unit_production_costs;
    }
    // in production extension
    return production_quantity * unit_production_costs + calc_production_extension_penalty(production_quantity);
}

auto SalesManager::calc_production_extension_penalty(const FlowQuantity& production_quantity) const -> FlowValue {
    assert(production_quantity >= 0.0);
    if (production_quantity <= firm->forced_baseline_production_quantity()) {  // not in production extension
        return FlowValue{0.0};
    }
    // in production extension
    if (production_quantity > firm->forced_maximal_production_quantity()) {
        log::info(this, production_quantity);
        log::info(this, firm->forced_maximal_production_quantity());
    }
    assert(production_quantity <= firm->forced_maximal_production_quantity());
    return firm->sector->parameters().price_increase_production_extension / (2 * firm->forced_baseline_production_quantity())
           * (production_quantity - firm->forced_baseline_production_quantity()) * (production_quantity - firm->forced_baseline_production_quantity());

    // maximal penalty per unit: price_increase_production_extension / 2 * (possible_overcapacity_ratio - 1)^2 / possible_overcapacity_ratio
}

auto SalesManager::calc_marginal_production_extension_penalty(const FlowQuantity& production_quantity) const -> Price {
    assert(production_quantity >= 0.0);
    if (production_quantity <= firm->forced_baseline_production_quantity()) {  // not in production extension
        return Price{0.0};
    }
    // in production extension
    return firm->sector->parameters().price_increase_production_extension / firm->forced_baseline_production_quantity()
           * (production_quantity - firm->forced_baseline_production_quantity());
    // maximal: price_increase_production_extension * (possible_overcapacity_ratio - 1)
}

auto SalesManager::calc_marginal_production_costs(const FlowQuantity& production_quantity, const Price& unit_production_costs) const -> Price {
    assert(production_quantity >= 0.0);
    assert(!isnan(unit_production_costs));
    if (production_quantity <= firm->forced_baseline_production_quantity()) {  // not in production extension
        return unit_production_costs;
    }
    // in production extension
    return unit_production_costs + calc_marginal_production_extension_penalty(production_quantity);
}

auto SalesManager::analytic_solution_in_production_extension(const Price& unit_production_costs, const Price& price_demand_request_not_served_completely) const
    -> FlowQuantity {
    debug::assertstepor(this, IterationStep::CONSUMPTION_AND_PRODUCTION, IterationStep::EXPECTATION);
    assert(price_demand_request_not_served_completely >= unit_production_costs);
    return round(
        firm->forced_baseline_production_quantity()
        * (1.0 + (price_demand_request_not_served_completely - unit_production_costs) / firm->sector->parameters().price_increase_production_extension));
}

auto SalesManager::calc_additional_revenue_expectation(const FlowQuantity& production_quantity_X, const Price& n_min) const -> FlowValue {
    // note: for X > sum_of_demand_requests_D this curve corresponds to the marginal supply curve
    // note: communicated_parameters_.production <= sum_demand_requests_D (up to rounding issues)
    assert(round(production_quantity_X) >= sum_demand_requests_.get_quantity());
    assert(round(production_quantity_X) <= round(communicated_parameters_.possible_production.get_quantity()));
    assert(firm->sector->parameters().supply_elasticity < 1.0);

    // fixed supply elasticity
    return n_min
           * (production_quantity_X * pow(sum_demand_requests_.get_quantity() / production_quantity_X, firm->sector->parameters().supply_elasticity)
              - sum_demand_requests_.get_quantity())
           / (1.0 - firm->sector->parameters().supply_elasticity);
}

auto SalesManager::calc_marginal_revenue_curve(const FlowQuantity& production_quantity_X, const Price& n_min) const -> Price {
    // note: for X > sum_of_demand_requests_D this curve corresponds to the marginal supply curve
    // note: communicated_parameters_.production == sum_demand_requests_D (up to rounding issues)
    assert(round(production_quantity_X) >= sum_demand_requests_.get_quantity());
    assert(round(production_quantity_X) <= communicated_parameters_.possible_production.get_quantity());
    assert(firm->sector->parameters().supply_elasticity < 1.0);

    return n_min * pow(sum_demand_requests_.get_quantity() / production_quantity_X, firm->sector->parameters().supply_elasticity);
}

auto SalesManager::goal_fkt_marginal_costs_minus_marginal_revenue(const FlowQuantity& production_quantity_X,
                                                                  const Price& unit_production_costs,
                                                                  const Price& n_min) const -> Price {
    FlowQuantity const production_quantity_X_rounded = round(production_quantity_X);
    assert(production_quantity_X_rounded <= communicated_parameters_.possible_production.get_quantity());
    const Price marginal_revenue = calc_marginal_revenue_curve(production_quantity_X_rounded, n_min);
    return calc_marginal_production_costs(production_quantity_X_rounded, unit_production_costs) - marginal_revenue;
}

auto SalesManager::goal_fkt_marginal_costs_minus_price(const FlowQuantity& production_quantity_X, const Price& unit_production_costs, const Price& price) const
    -> Price {
    FlowQuantity const production_quantity_X_rounded = round(production_quantity_X);
    return calc_marginal_production_costs(production_quantity_X_rounded, unit_production_costs) - price;
}

auto SalesManager::search_root_bisec_expectation(const FlowQuantity& left,
                                                 const FlowQuantity& right,
                                                 const FlowQuantity& production_quantity_X,
                                                 const Price& unit_production_costs,
                                                 const Price& n_min,
                                                 const Price& precision) const -> Flow {
    assert(left < right);  // check limits of interval
    // check if root is in interval

    FlowQuantity const middle = (left + right) / 2;
    if (left + FlowQuantity(FlowQuantity::precision) >= right) {  // interval too narrow
        if (abs(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X + left, unit_production_costs, n_min))
            < abs(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X + right, unit_production_costs, n_min))) {
            return {left, calc_additional_revenue_expectation(round(production_quantity_X + left), n_min)};
        }
        return {right, calc_additional_revenue_expectation(round(production_quantity_X + right), n_min)};
    }
    // check if root is in interval
    assert(!same_sgn(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X + left, unit_production_costs, n_min),
                     goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X + right, unit_production_costs, n_min)));

    if (abs(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X + middle, unit_production_costs, n_min)) < precision) {
        return {middle, calc_additional_revenue_expectation(production_quantity_X + middle, n_min)};
    }
    if (abs(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X + left, unit_production_costs, n_min)) < precision) {
        return {left, calc_additional_revenue_expectation(production_quantity_X + left, n_min)};
    }
    if (abs(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X + right, unit_production_costs, n_min)) < precision) {
        return {right, calc_additional_revenue_expectation(production_quantity_X + right, n_min)};
    }
    if (!same_sgn(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X + middle, unit_production_costs, n_min),
                  goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X + right, unit_production_costs, n_min))) {
        return search_root_bisec_expectation(middle, right, production_quantity_X, unit_production_costs, n_min, precision);
    }
    return search_root_bisec_expectation(left, middle, production_quantity_X, unit_production_costs, n_min, precision);
}

auto SalesManager::communicated_parameters() const -> const SupplyParameters& {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return communicated_parameters_;
}

auto SalesManager::total_production_costs() const -> const FlowValue& {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return total_production_costs_;
}

auto SalesManager::total_revenue() const -> const FlowValue& {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return total_revenue_;
}

void SalesManager::impose_tax(const Ratio tax_p) {
    debug::assertstep(this, IterationStep::EXPECTATION);
    tax_ = tax_p;
}

auto SalesManager::get_tax() const -> FlowValue { return tax_ * firm->production().get_value(); }

void SalesManager::debug_print_details() const {
    if constexpr (Options::DEBUGGING) {
        log::info(this, business_connections.size(), " outputs:");
        for (const auto& bc : business_connections) {
            log::info(this, "    ", bc->name(), "  Z_star= ", std::setw(11), bc->baseline_flow().get_quantity());
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

void SalesManager::print_parameters() const {
    if constexpr (Options::DEBUGGING) {
        log::info(this, "parameters:");
        print_row("X", communicated_parameters_.production.get_quantity());
        print_row("X_exp", communicated_parameters_.expected_production.get_quantity());
        print_row("X_hat", communicated_parameters_.possible_production);
        print_row("  lambda", firm->forcing());
        print_row("X_star", firm->baseline_production().get_quantity());
        print_row("lambda X_star", firm->forced_baseline_production_quantity());
        print_row("p", communicated_parameters_.production.get_quantity() / firm->baseline_production().get_quantity());
        print_row("p_exp", communicated_parameters_.expected_production.get_quantity() / firm->baseline_production().get_quantity());
        print_row("m_prod_cost(X)", calc_marginal_production_costs(communicated_parameters_.production.get_quantity(),
                                                                   communicated_parameters_.possible_production.get_price() * (1 - tax_)));
        print_row("expected m_prod_cost(X)", calc_marginal_production_costs(communicated_parameters_.expected_production.get_quantity(),
                                                                            communicated_parameters_.possible_production.get_price() * (1 - tax_)));
        print_row("n_bar", communicated_parameters_.offer_price);
        print_row("n_c", communicated_parameters_.possible_production.get_price());
        print_row("  C", total_production_costs_);
        std::cout << std::flush;
    }
}

void SalesManager::print_connections(std::vector<std::shared_ptr<BusinessConnection>>::const_iterator begin_equally_distributed,
                                     std::vector<std::shared_ptr<BusinessConnection>>::const_iterator end_equally_distributed) const {
    if constexpr (Options::DEBUGGING) {
#pragma omp critical(output)
        {
            std::cout << model()->run()->timeinfo() << ", " << name() << ": supply distribution for " << business_connections.size() << " outputs:\n";
            auto sum = FlowQuantity(0.0);
            auto initial_sum = FlowQuantity(0.0);
            for (const auto& bc : business_connections) {
                std::cout << "      " << bc->name() << " :\n";
                print_row("n", bc->last_demand_request().get_price());
                print_row("D_r", bc->last_demand_request().get_quantity());
                print_row("D_star", bc->baseline_flow().get_quantity());
                print_row("Z_last", bc->last_shipment(this).get_quantity());
                print_row("in_share", (bc->baseline_flow() / bc->buyer->storage->baseline_input_flow()));
                std::cout << '\n';
                sum += bc->last_demand_request().get_quantity();
                initial_sum += bc->baseline_flow().get_quantity();
            }
            if (supply_distribution_scenario.connection_not_served_completely != business_connections.end()) {
                std::cout << "      not completely served: " << (*supply_distribution_scenario.connection_not_served_completely)->name() << '\n';
            } else {
                std::cout << "      all connections served completely\n";
            }
            if (begin_equally_distributed != business_connections.end()) {
                std::cout << "      first_equal_distribution: " << (*begin_equally_distributed)->name() << '\n';
            }
            if (end_equally_distributed != business_connections.end()) {
                std::cout << "      last_equal_distribution:  " << (*end_equally_distributed)->name() << '\n';
            }
            std::cout << "      Sums:\n";
            print_row("sum D_r", sum);
            print_row("sum_star D_r", initial_sum);
            std::cout << std::flush;
        }
    }
}

}  // namespace acclimate
