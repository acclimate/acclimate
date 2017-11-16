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

#include "model/SalesManagerPrices.h"
#include <algorithm>
#include "model/BusinessConnection.h"
#include "model/CapacityManagerPrices.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/Storage.h"
#include "model/TransportChainLink.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
SalesManagerPrices<ModelVariant>::SalesManagerPrices(Firm<ModelVariant>* firm_p) : SalesManager<ModelVariant>(firm_p) {}

template<class ModelVariant>
const Flow SalesManagerPrices<ModelVariant>::calc_production_X() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    assert(!business_connections.empty());
    sum_demand_requests_D_ = round(sum_demand_requests_D_);

    // sort all incoming connections by price (descending), then by quantity (descending)
    std::sort(
        business_connections.begin(), business_connections.end(),
        [](const std::unique_ptr<BusinessConnection<ModelVariant>>& business_connection_1,
           const std::unique_ptr<BusinessConnection<ModelVariant>>& business_connection_2) {
            if (business_connection_1->last_demand_request_D().get_quantity() <= 0.0 && business_connection_2->last_demand_request_D().get_quantity() > 0.0) {
                // we want to store empty demand requests at the end of the business connections
                return false;
            }
            if (business_connection_1->last_demand_request_D().get_quantity() > 0.0 && business_connection_2->last_demand_request_D().get_quantity() <= 0.0) {
                return true;
            }
            if (business_connection_1->last_demand_request_D().get_quantity() <= 0.0 && business_connection_2->last_demand_request_D().get_quantity() <= 0.0) {
                // both are empty --> ordering does not matter
                return false;
            }
            if (business_connection_1->last_demand_request_D().get_price() > business_connection_2->last_demand_request_D().get_price()) {
                return true;
            }
            if (business_connection_1->last_demand_request_D().get_price() < business_connection_2->last_demand_request_D().get_price()) {
                return false;
            }
            // none of them is empty, but prices are equal
            return (business_connection_1->last_demand_request_D().get_quantity() > business_connection_2->last_demand_request_D().get_quantity());
        });

    Flow possible_production_X_hat = firm->capacity_manager->possible_production_X_hat();
    if (estimated_possible_production_X_hat_.get_quantity() > 0.0) {
        // price (unit_production_costs_n_c) has been calculated already in estimation step
        possible_production_X_hat.set_price(estimated_possible_production_X_hat_.get_price());
    }
    std::tie(communicated_parameters_.production_X, communicated_parameters_.offer_price_n_bar) = calc_supply_distribution_scenario(possible_production_X_hat);
    communicated_parameters_.possible_production_X_hat = possible_production_X_hat;

    return communicated_parameters_.production_X;
}

template<class ModelVariant>
void SalesManagerPrices<ModelVariant>::distribute(const Flow& _) {
    UNUSED(_);
    assertstep(CONSUMPTION_AND_PRODUCTION);
    assert(!business_connections.empty());
    // push all flows
    if (communicated_parameters_.production_X.get_quantity() <= 0.0) {  // no production
        for (auto not_served_bc = business_connections.begin(); not_served_bc != business_connections.end(); ++not_served_bc) {
            (*not_served_bc)->push_flow_Z(Flow(0.0));
        }
    } else {  // non-zero production to distribute
#ifdef DEBUG
        unsigned int pushed_flows = 0;
#endif
        assert(!isnan(supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization));
        Price cheapest_price_range_half_width = Price(0.0);
        if (firm->sector->model->parameters().cheapest_price_range_generic_size) {
            cheapest_price_range_half_width = firm->sector->parameters().price_increase_production_extension / 2
                                              * (firm->capacity_manager->possible_overcapacity_ratio_beta - 1)
                                              * (firm->capacity_manager->possible_overcapacity_ratio_beta - 1)
                                              / firm->capacity_manager->possible_overcapacity_ratio_beta;  // = maximal penalty per unit
        } else {
            cheapest_price_range_half_width = firm->sector->model->parameters().cheapest_price_range_width / 2;
        }
        auto begin_cheapest_price_range = business_connections.begin();
        auto end_cheapest_price_range = business_connections.begin();
        FlowQuantity demand_cheapest_price_range = FlowQuantity(0.0);
        FlowValue demand_value_cheapest_price_range = FlowValue(0.0);
        Flow production_without_cheapest_price_range = Flow(0.0);
        for (auto served_bc = business_connections.begin(); served_bc != business_connections.end(); ++served_bc) {
            if ((*served_bc)->last_demand_request_D().get_quantity() > 0.0) {
                if (round((*served_bc)->last_demand_request_D().get_price() - supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization)
                    >= cheapest_price_range_half_width) {  // price higher than in cheapest price range
                    assert((*served_bc)->last_demand_request_D().get_quantity() <= communicated_parameters_.production_X.get_quantity());
                    (*served_bc)->push_flow_Z(round((*served_bc)->last_demand_request_D()));
                    production_without_cheapest_price_range += round((*served_bc)->last_demand_request_D());
#ifdef DEBUG
                    pushed_flows++;
#endif
                    begin_cheapest_price_range = served_bc + 1;
                } else if (abs(round((*served_bc)->last_demand_request_D().get_price()
                                     - supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization))
                           < cheapest_price_range_half_width) {  // price in cheapest price range
                    demand_cheapest_price_range += (*served_bc)->last_demand_request_D().get_quantity();
                    demand_value_cheapest_price_range += (*served_bc)->last_demand_request_D().get_value();
                    // flow is pushed later
                    end_cheapest_price_range = served_bc + 1;
                } else {  // price lower than in cheapest price range
                    (*served_bc)->push_flow_Z(Flow(0.0));
#ifdef DEBUG
                    pushed_flows++;
#endif
                }
            } else {  // demand request is zero
                (*served_bc)->push_flow_Z(Flow(0.0));
#ifdef DEBUG
                pushed_flows++;
#endif
            }
        }
        if (begin_cheapest_price_range < end_cheapest_price_range) {
            const FlowQuantity production_cheapest_price_range =
                round(communicated_parameters_.production_X.get_quantity() - production_without_cheapest_price_range.get_quantity());
            if (demand_cheapest_price_range > production_cheapest_price_range) {  // not all demands in cheapest price range can be fulfilled
                const Price max_price_cheapest_price_range =
                    supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization
                    + cheapest_price_range_half_width;  // (*begin_cheapest_price_range)->last_demand_request_D().get_price();
                const Price min_price_cheapest_price_range =
                    supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization
                    - cheapest_price_range_half_width;  // (*(end_cheapest_price_range - 1))->last_demand_request_D().get_price();
                const Price average_price_cheapest_price_range = demand_value_cheapest_price_range / demand_cheapest_price_range;
                const Price price_shift = std::max(
                    (max_price_cheapest_price_range * production_cheapest_price_range - average_price_cheapest_price_range * demand_cheapest_price_range)
                        / (demand_cheapest_price_range - production_cheapest_price_range),
                    Price(0.0) - min_price_cheapest_price_range);
                const Price price_as_calculated_in_distribution_scenario =
                    (communicated_parameters_.production_X.get_value() - production_without_cheapest_price_range.get_value()) / production_cheapest_price_range;

                total_revenue_R_ = production_without_cheapest_price_range.get_value();
                // serve demand requests in equal distribution range
                for (auto served_bc = begin_cheapest_price_range; served_bc != end_cheapest_price_range; ++served_bc) {
                    Flow flow_Z =
                        Flow(round(FlowQuantity(to_float(production_cheapest_price_range)
                                                * to_float((*served_bc)->last_demand_request_D().get_quantity() * price_shift
                                                           + (*served_bc)->last_demand_request_D().get_value())
                                                / to_float(demand_cheapest_price_range * price_shift + demand_value_cheapest_price_range))),
                             firm->sector->model->parameters().cheapest_price_range_preserve_seller_price ? price_as_calculated_in_distribution_scenario
                                                                                                          : (*served_bc)->last_demand_request_D().get_price());
                    assert(flow_Z.get_quantity() <= (*served_bc)->last_demand_request_D().get_quantity());
                    (*served_bc)->push_flow_Z(flow_Z);
                    total_revenue_R_ += flow_Z.get_value();
#ifdef DEBUG
                    pushed_flows++;
#endif
                }
            } else {  // all demands in cheapest price range can be fulfilled
                total_revenue_R_ = communicated_parameters_.production_X.get_value();
                for (auto served_bc = begin_cheapest_price_range; served_bc != end_cheapest_price_range; ++served_bc) {
                    assert((*served_bc)->last_demand_request_D().get_quantity() <= communicated_parameters_.production_X.get_quantity());
                    (*served_bc)->push_flow_Z(round((*served_bc)->last_demand_request_D()));
#ifdef DEBUG
                    pushed_flows++;
#endif
                }
            }
        } else {
            total_revenue_R_ = communicated_parameters_.production_X.get_value();
        }
#ifdef DEBUG
        if (pushed_flows < business_connections.size()) {
            debug(pushed_flows);
            debug(business_connections.size());
        }
        assert(pushed_flows == business_connections.size());
#endif
    }
}

template<class ModelVariant>
std::tuple<Flow, Price> SalesManagerPrices<ModelVariant>::calc_supply_distribution_scenario(const Flow& possible_production_X_hat_p) {
    assertstep(CONSUMPTION_AND_PRODUCTION);

    // find first (in order) connection with zero demand
    auto first_zero_connection = std::find_if(business_connections.begin(), business_connections.end(),
                                              [](const std::unique_ptr<BusinessConnection<ModelVariant>>& business_connection) {
                                                  return business_connection->last_demand_request_D().get_quantity() <= 0.0;
                                              });

    supply_distribution_scenario.connection_not_served_completely = first_zero_connection;
    total_production_costs_C_ = FlowValue(0.0);
    Price minimal_production_price = round(possible_production_X_hat_p.get_price());
    Price minimal_offer_price = round(minimal_production_price + get_initial_markup() / (1 - tax_));

    if (possible_production_X_hat_p.get_quantity() <= 0.0) {
        // no production due to supply shortage or forcing == 0

        if (firm->forcing() <= 0.0) {
            warning("no production due to total forcing");
        } else {
            Acclimate::Run<ModelVariant>::instance()->event(EventType::NO_PRODUCTION_SUPPLY_SHORTAGE, firm);
        }
        supply_distribution_scenario.connection_not_served_completely = business_connections.begin();
        supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
        supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization = Price::quiet_NaN();
        return std::make_tuple(Flow(0.0), Price::quiet_NaN());
    }
    if (first_zero_connection == business_connections.begin()  // no production due to demand quantity shortage
        || round((*business_connections.begin())->last_demand_request_D().get_price()) < minimal_production_price  // no production due to demand value shortage
    ) {
        // no production due to demand shortage

        if (first_zero_connection == business_connections.begin()) {
            Acclimate::Run<ModelVariant>::instance()->event(EventType::NO_PRODUCTION_DEMAND_QUANTITY_SHORTAGE, firm);
        } else {
            Acclimate::Run<ModelVariant>::instance()->event(EventType::NO_PRODUCTION_DEMAND_VALUE_SHORTAGE, firm);
        }
        supply_distribution_scenario.connection_not_served_completely = business_connections.begin();
        supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
        // communicate least price we would produce for:
        supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization = Price::quiet_NaN();
        assert(!isnan(minimal_production_price));
        return std::make_tuple(Flow(0.0), Price::quiet_NaN());
    }
    // non-zero production

    Flow production_X = Flow(0.0);

    // cycle over non-zero connections
    for (auto business_connection = business_connections.begin(); business_connection != first_zero_connection; ++business_connection) {
        // check that connection can be served at all
        if (round((production_X + (*business_connection)->last_demand_request_D())).get_quantity() > possible_production_X_hat_p.get_quantity()) {
            supply_distribution_scenario.connection_not_served_completely = business_connection;
            break;
        }
        // check that price of demand request is high enough
        Price maximal_marginal_production_costs =
            calc_marginal_production_costs((round(production_X + (*business_connection)->last_demand_request_D())).get_quantity(), minimal_production_price);
        if (isnan(maximal_marginal_production_costs)  // infinite costs
            || round((*business_connection)->last_demand_request_D().get_price()) < round(maximal_marginal_production_costs)) {
            supply_distribution_scenario.connection_not_served_completely = business_connection;
            break;
        }
        production_X += (*business_connection)->last_demand_request_D();
    }
    production_X = round(production_X);
    assert(production_X.get_quantity() <= possible_production_X_hat_p.get_quantity());

    if (supply_distribution_scenario.connection_not_served_completely == first_zero_connection) {
        // all demand requests have been served completely to the price they offered
        auto cheapest_served_non_zero_connection = first_zero_connection - 1;  // note: first_zero_connection == business_connections.begin() is excluded
        assert(production_X.get_quantity() <= sum_demand_requests_D_.get_quantity());  // to see if we make rounding errors
        assert(production_X.get_quantity() >= sum_demand_requests_D_.get_quantity());  // to see if we make rounding errors
        total_production_costs_C_ = calc_total_production_costs(production_X, minimal_production_price);
        // check for supply limitation offer price is reduced to attract more demand
        if (round(firm->capacity_manager->desired_production_X_tilde().get_quantity())
                < firm->forced_initial_production_quantity_lambda_X_star()  // not in production extension and ...
            && round(firm->capacity_manager->desired_production_X_tilde().get_quantity())
                   < round(possible_production_X_hat_p.get_quantity())  // ... and demand quantity limited
        ) {                                                             // note: round needed, otherwise the loop is entered also in equilibrium
            // demand quantity limitation
            assert(!isnan(minimal_production_price));
            // linear price reduction (is overwritten when considering expectation)
            const Price offer_price_n_bar = round(std::max(
                production_X.get_price()
                    * (1
                       + firm->sector->parameters().supply_elasticity
                             * ((production_X - (firm->forced_initial_production_lambda_X_star())) / (firm->forced_initial_production_lambda_X_star()))),
                minimal_offer_price));
            supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization =
                (*cheapest_served_non_zero_connection)->last_demand_request_D().get_price();
            return std::make_tuple(production_X, offer_price_n_bar);
        }
        // no demand quantity limitation sum_D >= lambda * X_star
        const Price offer_price_n_bar = round(production_X.get_price());
        supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization =
            round((*cheapest_served_non_zero_connection)->last_demand_request_D().get_price());
        return std::make_tuple(production_X, offer_price_n_bar);
    }
    // not all demand requests have been served completely
    if (round(goal_fkt_marginal_costs_minus_price(production_X.get_quantity(), minimal_production_price,
                                                  (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request_D().get_price()))
        < 0.0) {
        // we will send a non-zero amount to connection_not_served_completely

        if (round(goal_fkt_marginal_costs_minus_price(possible_production_X_hat_p.get_quantity(), minimal_production_price,
                                                      (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request_D().get_price()))
            < 0.0)
        // price is high enough, we would send as much as we can to the purchaser
        {
            supply_distribution_scenario.flow_not_served_completely =
                round(Flow(possible_production_X_hat_p.get_quantity() - production_X.get_quantity(),
                           (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request_D().get_price()));
            assert(supply_distribution_scenario.flow_not_served_completely.get_quantity() >= 0.0);
            supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization =
                (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request_D().get_price();
        } else {  // not supply limited
            // price is not high enough to fulfill complete demand request, we have to find the optimal production ratio
            FlowQuantity total_production_quantity = analytic_solution_in_production_extension(
                minimal_production_price, (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request_D().get_price());
            assert(total_production_quantity >= production_X.get_quantity());

            if (total_production_quantity > production_X.get_quantity()) {
                supply_distribution_scenario.flow_not_served_completely =
                    round(Flow(total_production_quantity - production_X.get_quantity(),
                               (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request_D().get_price()));
                assert(supply_distribution_scenario.flow_not_served_completely.get_quantity() >= 0.0);
                supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization =
                    (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request_D().get_price();
            } else {  // zero flow is sent
                supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
                if (supply_distribution_scenario.connection_not_served_completely == business_connections.begin()) {
                    // we have actually no production at all
                    // offer_price and cutoff_price will be set below
                    assert(production_X.get_quantity() <= 0.0);
                } else {
                    auto cheapest_served_connection = supply_distribution_scenario.connection_not_served_completely - 1;
                    // note: connection_not_served_completely != business_connections.begin(), otherwise: no production
                    supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization =
                        (*cheapest_served_connection)->last_demand_request_D().get_price();
                }
            }
        }
        production_X += supply_distribution_scenario.flow_not_served_completely;
        production_X = round(production_X);
        assert(production_X.get_quantity() <= possible_production_X_hat_p.get_quantity());
    } else {  // we will send a zero-amount to connection_not_served_completely
        supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
        if (supply_distribution_scenario.connection_not_served_completely != business_connections.begin()) {
            // we have a non-zero production
            auto cheapest_served_connection = supply_distribution_scenario.connection_not_served_completely - 1;
            // note: connection_not_served_completely != business_connections.begin(), otherwise: no production
            supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization = (*cheapest_served_connection)->last_demand_request_D().get_price();
        }
    }
    total_production_costs_C_ = calc_total_production_costs(production_X, minimal_production_price);
    if (production_X.get_quantity() > 0.0) {
        // we have a non-zero production
        const Price offer_price_n_bar = round(production_X.get_price());
        return std::make_tuple(production_X, offer_price_n_bar);
    }
    // no production
    Acclimate::Run<ModelVariant>::instance()->event(EventType::NO_PRODUCTION_HIGH_COSTS, firm);
    warning("no production because of high costs / too low priced demand requests");
    supply_distribution_scenario.connection_not_served_completely = business_connections.begin();
    supply_distribution_scenario.flow_not_served_completely = Flow(0.0);

    // communicate least price we would produce for:
    supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization = Price::quiet_NaN();
    const Price& offer_price_n_bar = minimal_production_price;
    return std::make_tuple(production_X, offer_price_n_bar);
}

template<class ModelVariant>
std::tuple<Flow, Price> SalesManagerPrices<ModelVariant>::calc_expected_supply_distribution_scenario(const Flow& possible_production_X_hat_p) {
    assertstep(EXPECTATION);
    // find first (in order) connection with zero demand
    auto first_zero_connection = std::find_if(business_connections.begin(), business_connections.end(),
                                              [](const std::unique_ptr<BusinessConnection<ModelVariant>>& business_connection) {
                                                  return business_connection->last_demand_request_D().get_quantity() <= 0.0;
                                              });

    supply_distribution_scenario.connection_not_served_completely = first_zero_connection;
    Price minimal_production_price = round(possible_production_X_hat_p.get_price());
    Price minimal_offer_price = round(minimal_production_price + get_initial_markup() / (1 - tax_));

    if (possible_production_X_hat_p.get_quantity() <= 0.0) {
        // no production due to supply shortage or forcing <= 0

        if (firm->forcing() <= 0.0) {
            warning("no expected production due to total forcing");
        } else {
            Acclimate::Run<ModelVariant>::instance()->event(EventType::NO_EXP_PRODUCTION_SUPPLY_SHORTAGE, firm);
        }
        supply_distribution_scenario.connection_not_served_completely = business_connections.begin();
        supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
        supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization = Price::quiet_NaN();
        return std::make_tuple(Flow(0.0), Price::quiet_NaN());
    }
    if (first_zero_connection == business_connections.begin()  // no production due to demand quantity shortage
        || round((*business_connections.begin())->last_demand_request_D().get_price()) < minimal_production_price) {
        // no production due to demand shortage

        if (first_zero_connection == business_connections.begin()) {
            Acclimate::Run<ModelVariant>::instance()->event(EventType::NO_EXP_PRODUCTION_DEMAND_QUANTITY_SHORTAGE, firm);
        } else {
            Acclimate::Run<ModelVariant>::instance()->event(EventType::NO_EXP_PRODUCTION_DEMAND_VALUE_SHORTAGE, firm);
        }
        supply_distribution_scenario.connection_not_served_completely = business_connections.begin();
        supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
        // communicate least price we would produce for:
        supply_distribution_scenario.price_cheapest_buyer_accepted_in_optimization = Price::quiet_NaN();
        assert(!isnan(minimal_production_price));
        const Price& offer_price_n_bar = minimal_production_price;
        Flow expected_production_X =
            Flow(std::min(firm->forced_initial_production_lambda_X_star().get_quantity(), possible_production_X_hat_p.get_quantity()), offer_price_n_bar);
        return std::make_tuple(expected_production_X, offer_price_n_bar);
    }
    // non-zero expected production

    Flow expected_production_X = Flow(0.0);

    // cycle over non-zero connections
    for (auto business_connection = business_connections.begin(); business_connection != first_zero_connection; business_connection++) {
        // check that connection can be served under quantitative aspects
        if (round((expected_production_X + (*business_connection)->last_demand_request_D())).get_quantity() > possible_production_X_hat_p.get_quantity()) {
            supply_distribution_scenario.connection_not_served_completely = business_connection;
            break;
        }
        // check that price of demand request is high enough
        Price maximal_marginal_production_costs = Price(0.0);

        if (!firm->sector->model->parameters().respect_markup_in_production_extension                           // we always want to attract more demand
            || expected_production_X.get_quantity() < firm->forced_initial_production_quantity_lambda_X_star()  // ... production is below lambda * X_star
        ) {
            maximal_marginal_production_costs = calc_marginal_production_costs(
                (round(expected_production_X + (*business_connection)->last_demand_request_D())).get_quantity(), minimal_production_price);
        } else {
            maximal_marginal_production_costs = calc_marginal_production_costs(
                (round(expected_production_X + (*business_connection)->last_demand_request_D())).get_quantity(), minimal_offer_price);
        }
        if (isnan(maximal_marginal_production_costs)  // infinite costs
            || round((*business_connection)->last_demand_request_D().get_price()) < round(maximal_marginal_production_costs)) {
            supply_distribution_scenario.connection_not_served_completely = business_connection;
            break;
        }
        expected_production_X += (*business_connection)->last_demand_request_D();
    }
    expected_production_X = round(expected_production_X);
    assert(expected_production_X.get_quantity() <= possible_production_X_hat_p.get_quantity());

    if (supply_distribution_scenario.connection_not_served_completely == first_zero_connection) {
        // all demand requests would be served completely to the price they offered
        auto cheapest_served_non_zero_connection = first_zero_connection - 1;  // note: first_zero_connection == business_connections.begin() is excluded
        // Expectations: the demand curves have to be extended
        // check that we reach the regime where X > sum_of_demand_requests

        const bool respect_markup_in_extension_of_revenue_curve = true;

        if ((firm->sector->model->parameters().always_extend_expected_demand_curve                               // we always want to attract more demand
             || expected_production_X.get_quantity() < firm->forced_initial_production_quantity_lambda_X_star()  // ... production is below lambda * X_star
             )
            && !firm->sector->model->parameters().naive_expectations) {
            if (round(goal_fkt_marginal_costs_minus_marginal_revenue(
                    expected_production_X.get_quantity(), respect_markup_in_extension_of_revenue_curve ? minimal_offer_price : minimal_production_price,
                    (*cheapest_served_non_zero_connection)->last_demand_request_D().get_price()))
                < 0.0) {
                // additional_expected_flow is non-zero

                Flow additional_expected_flow = Flow(0.0);
                FlowQuantity maximal_additional_production_quantity =
                    round(std::min(possible_production_X_hat_p.get_quantity(), firm->forced_maximal_production_quantity_lambda_beta_X_star())
                          - expected_production_X.get_quantity());
                if (round(goal_fkt_marginal_costs_minus_marginal_revenue(
                        round(expected_production_X.get_quantity() + maximal_additional_production_quantity),
                        respect_markup_in_extension_of_revenue_curve ? minimal_offer_price : minimal_production_price,
                        (*cheapest_served_non_zero_connection)->last_demand_request_D().get_price()))
                    < 0.0) {
                    // price is high enough, we would provide as much as we can to the purchasers
                    additional_expected_flow =
                        Flow(maximal_additional_production_quantity,
                             calc_additional_revenue_expectation(round(expected_production_X.get_quantity() + maximal_additional_production_quantity),
                                                                 (*cheapest_served_non_zero_connection)->last_demand_request_D().get_price()));
                } else {
                    // price is not high enough to fulfill complete demand request, we have to find the optimal production ratio
                    Price precision_root_finding = Price(Price::precision);
                    additional_expected_flow =
                        search_root_bisec_expectation(FlowQuantity(0.0), maximal_additional_production_quantity, expected_production_X.get_quantity(),
                                                      respect_markup_in_extension_of_revenue_curve ? minimal_offer_price : minimal_production_price,
                                                      (*cheapest_served_non_zero_connection)->last_demand_request_D().get_price(),
                                                      precision_root_finding);  // note: true: initial markup respected
                    assert(round(additional_expected_flow).get_quantity() >= 0.0);
                }
                expected_production_X = round(expected_production_X + additional_expected_flow);
            }
        }

        // n_bar respects markup
        const Price offer_price_n_bar = round(std::max(
            expected_production_X.get_price(), calc_total_production_costs(expected_production_X, minimal_offer_price) / expected_production_X.get_quantity()));
        // n_co respects production costs
        assert(expected_production_X.get_quantity() <= possible_production_X_hat_p.get_quantity());
        return std::make_tuple(expected_production_X, offer_price_n_bar);
    }
    // not all demand requests would be served completely
    Price minimal_price = Price(0.0);
    if (!firm->sector->model->parameters().respect_markup_in_production_extension                           // we always want to attract more demand
        || expected_production_X.get_quantity() < firm->forced_initial_production_quantity_lambda_X_star()  // ... production is below lambda * X_star
    ) {
        minimal_price = minimal_production_price;
    } else {
        minimal_price = minimal_offer_price;
    }
    if (round(goal_fkt_marginal_costs_minus_price(expected_production_X.get_quantity(), minimal_price,
                                                  (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request_D().get_price()))
        < 0.0) {
        // price of connection_not_served_completely is high enough, we would send non-zero amount

        if (round(goal_fkt_marginal_costs_minus_price(possible_production_X_hat_p.get_quantity(), minimal_price,
                                                      (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request_D().get_price()))
            < 0.0)
        // price is high enough, we would send as much as we can to the purchaser
        {
            supply_distribution_scenario.flow_not_served_completely =
                round(Flow(possible_production_X_hat_p.get_quantity() - expected_production_X.get_quantity(),
                           (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request_D().get_price()));
            assert(supply_distribution_scenario.flow_not_served_completely.get_quantity() >= 0.0);
        } else {
            // price is not high enough to fulfill complete
            // demand request, we have to find the optimal
            // production ratio
            FlowQuantity total_production_quantity = analytic_solution_in_production_extension(
                minimal_price, (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request_D().get_price());
            assert(total_production_quantity >= expected_production_X.get_quantity());

            if (total_production_quantity > expected_production_X.get_quantity()) {
                // production_not_served_completely would get a non-zero amount
                supply_distribution_scenario.flow_not_served_completely =
                    Flow(total_production_quantity - expected_production_X.get_quantity(),
                         (*supply_distribution_scenario.connection_not_served_completely)->last_demand_request_D().get_price());
            } else {  // zero flow would be sent to production_not_served_completely
                supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
            }
        }
        expected_production_X += supply_distribution_scenario.flow_not_served_completely;
        expected_production_X = round(expected_production_X);
        assert(expected_production_X.get_quantity() <= possible_production_X_hat_p.get_quantity());
    } else {  // we would send a zero-amount to connection_not_served_completely
        supply_distribution_scenario.flow_not_served_completely = Flow(0.0);
    }
    if (expected_production_X.get_quantity() > 0.0) {
        // we have a non-zero production
        // n_bar respects markup and nco was set above
        const Price offer_price_n_bar = round(std::max(
            expected_production_X.get_price(), calc_total_production_costs(expected_production_X, minimal_offer_price) / expected_production_X.get_quantity()));
        return std::make_tuple(expected_production_X, offer_price_n_bar);
    }
    // zero production
    Acclimate::Run<ModelVariant>::instance()->event(EventType::NO_EXP_PRODUCTION_HIGH_COSTS, firm);
    // communicate least price we would produce for:
    const Price offer_price_n_bar = minimal_offer_price;
    return std::make_tuple(expected_production_X, offer_price_n_bar);
}

template<class ModelVariant>
void SalesManagerPrices<ModelVariant>::iterate_expectation() {
    assertstep(EXPECTATION);
    estimated_possible_production_X_hat_ = firm->capacity_manager->estimate_possible_production_X_hat();
    if (estimated_possible_production_X_hat_.get_quantity() > 0.0) {
        estimated_possible_production_X_hat_.set_price(estimated_possible_production_X_hat_.get_price()
                                                       / (1 - tax_));  // tax is applied here (1-tax_) because tax is on revenue!
    }
    std::tie(communicated_parameters_.expected_production_X, communicated_parameters_.offer_price_n_bar) =
        calc_expected_supply_distribution_scenario(estimated_possible_production_X_hat_);
    sum_demand_requests_D_ = Flow(0.0);
}

template<class ModelVariant>
inline const Price SalesManagerPrices<ModelVariant>::get_initial_unit_variable_production_costs() const {
    return std::max(Price(0.0), Price(1.0) - (initial_unit_commodity_costs + get_initial_markup()));
}

template<class ModelVariant>
inline const Price SalesManagerPrices<ModelVariant>::get_initial_markup() const {
    return std::min(Price(1.0) - initial_unit_commodity_costs, firm->sector->parameters().initial_markup);
}

template<class ModelVariant>
void SalesManagerPrices<ModelVariant>::initialize() {
    assertstep(INITIALIZATION);
    initial_unit_commodity_costs = Price(0.0);
    for (auto input_storage = firm->input_storages.begin(); input_storage != firm->input_storages.end(); ++input_storage) {
        initial_unit_commodity_costs += Price(1.0) * (*input_storage)->get_technology_coefficient_a();
    }
    assert(initial_unit_commodity_costs > 0.0);
    assert(initial_unit_commodity_costs <= 1);
    estimated_possible_production_X_hat_ = firm->maximal_production_beta_X_star();
    // actually only needed if we calculate possible_production_X_hat in estimation step (but we already need it in production step)
    estimated_possible_production_X_hat_.set_price(initial_unit_commodity_costs + get_initial_unit_variable_production_costs());
}

template<class ModelVariant>
const FlowValue SalesManagerPrices<ModelVariant>::calc_total_production_costs(const Flow& production_X, const Price& unit_production_costs_n_c) const {
    const FlowQuantity& production_quantity_X = production_X.get_quantity();
    assert(production_quantity_X >= 0.0);
    assert(!isnan(unit_production_costs_n_c));
    if (production_quantity_X <= firm->forced_initial_production_quantity_lambda_X_star()) {  // not in production extension
        return production_quantity_X * unit_production_costs_n_c;
    }
    // in production extension
    return production_quantity_X * unit_production_costs_n_c + calc_production_extension_penalty_P(production_quantity_X);
}

template<class ModelVariant>
const FlowValue SalesManagerPrices<ModelVariant>::calc_production_extension_penalty_P(const FlowQuantity& production_quantity_X) const {
    assert(production_quantity_X >= 0.0);
    if (production_quantity_X <= firm->forced_initial_production_quantity_lambda_X_star()) {  // not in production extension
        return FlowValue(0.0);
    }
    // in production extension
    if (production_quantity_X > firm->forced_maximal_production_quantity_lambda_beta_X_star()) {
        debug(production_quantity_X);
        debug(firm->forced_maximal_production_quantity_lambda_beta_X_star());
    }
    assert(production_quantity_X <= firm->forced_maximal_production_quantity_lambda_beta_X_star());
    return firm->sector->parameters().price_increase_production_extension / (2.0 * firm->forced_initial_production_quantity_lambda_X_star())
           * (production_quantity_X - firm->forced_initial_production_quantity_lambda_X_star())
           * (production_quantity_X - firm->forced_initial_production_quantity_lambda_X_star());

    // maximal penalty per unit: price_increase_production_extension / 2 * (possible_overcapacity_ratio - 1)^2 / possible_overcapacity_ratio
}

template<class ModelVariant>
const Price SalesManagerPrices<ModelVariant>::calc_marginal_production_extension_penalty(const FlowQuantity& production_quantity_X) const {
    assert(production_quantity_X >= 0.0);
    if (production_quantity_X <= firm->forced_initial_production_quantity_lambda_X_star()) {  // not in production extension
        return Price(0.0);
    }
    // in production extension
    return firm->sector->parameters().price_increase_production_extension / firm->forced_initial_production_quantity_lambda_X_star()
           * (production_quantity_X - firm->forced_initial_production_quantity_lambda_X_star());
    // maximal: price_increase_production_extension * (possible_overcapacity_ratio - 1)
}

template<class ModelVariant>
const Price SalesManagerPrices<ModelVariant>::calc_marginal_production_costs(const FlowQuantity& production_quantity_X,
                                                                             const Price& unit_production_costs_n_c) const {
    assert(production_quantity_X >= 0.0);
    assert(!isnan(unit_production_costs_n_c));
    if (production_quantity_X <= firm->forced_initial_production_quantity_lambda_X_star()) {  // not in production extension
        return unit_production_costs_n_c;
    }
    // in production extension
    return unit_production_costs_n_c + calc_marginal_production_extension_penalty(production_quantity_X);
}

template<class ModelVariant>
const FlowQuantity SalesManagerPrices<ModelVariant>::analytic_solution_in_production_extension(const Price& unit_production_costs_n_c,
                                                                                               const Price& price_demand_request_not_served_completely) const {
    assertstepor(CONSUMPTION_AND_PRODUCTION, EXPECTATION);
    assert(price_demand_request_not_served_completely >= unit_production_costs_n_c);
    return round(
        firm->forced_initial_production_quantity_lambda_X_star()
        * (1.0 + (price_demand_request_not_served_completely - unit_production_costs_n_c) / firm->sector->parameters().price_increase_production_extension));
}

template<class ModelVariant>
const FlowValue SalesManagerPrices<ModelVariant>::calc_additional_revenue_expectation(const FlowQuantity& production_quantity_X_p, const Price& n_min_p) const {
    // note: for X > sum_of_demand_requests_D this curve corresponds to the marginal supply curve
    // note: communicated_parameters_.production_X <= sum_demand_requests_D (up to rounding issues)
    assert(round(production_quantity_X_p) >= sum_demand_requests_D_.get_quantity());
    assert(round(production_quantity_X_p) <= round(communicated_parameters_.possible_production_X_hat.get_quantity()));
    assert(firm->sector->parameters().supply_elasticity < 1.0);

    // fixed supply elasticity
    return n_min_p
           * (production_quantity_X_p * pow(sum_demand_requests_D_.get_quantity() / production_quantity_X_p, firm->sector->parameters().supply_elasticity)
              - sum_demand_requests_D_.get_quantity())
           / (1.0 - firm->sector->parameters().supply_elasticity);
}

template<class ModelVariant>
const Price SalesManagerPrices<ModelVariant>::calc_marginal_revenue_curve(const FlowQuantity& production_quantity_X_p, const Price& n_min_p) const {
    // note: for X > sum_of_demand_requests_D this curve corresponds to the marginal supply curve
    // note: communicated_parameters_.production_X == sum_demand_requests_D (up to rounding issues)
    assert(round(production_quantity_X_p) >= sum_demand_requests_D_.get_quantity());
    assert(round(production_quantity_X_p) <= communicated_parameters_.possible_production_X_hat.get_quantity());
    assert(firm->sector->parameters().supply_elasticity < 1.0);

    return n_min_p * pow(sum_demand_requests_D_.get_quantity() / production_quantity_X_p, firm->sector->parameters().supply_elasticity);
}

template<class ModelVariant>
const Price SalesManagerPrices<ModelVariant>::goal_fkt_marginal_costs_minus_marginal_revenue(const FlowQuantity& production_quantity_X_p,
                                                                                             const Price& unit_production_costs_n_c,
                                                                                             const Price& n_min_p) const {
    FlowQuantity production_quantity_X_rounded = round(production_quantity_X_p);
    assert(production_quantity_X_rounded <= communicated_parameters_.possible_production_X_hat.get_quantity());
    const Price marginal_revenue = calc_marginal_revenue_curve(production_quantity_X_rounded, n_min_p);
    return calc_marginal_production_costs(production_quantity_X_rounded, unit_production_costs_n_c) - marginal_revenue;
}

template<class ModelVariant>
const Price SalesManagerPrices<ModelVariant>::goal_fkt_marginal_costs_minus_price(const FlowQuantity& production_quantity_X_p,
                                                                                  const Price& unit_production_costs_n_c,
                                                                                  const Price& price) const {
    FlowQuantity production_quantity_X_rounded = round(production_quantity_X_p);
    return calc_marginal_production_costs(production_quantity_X_rounded, unit_production_costs_n_c) - price;
}

template<class ModelVariant>
const Flow SalesManagerPrices<ModelVariant>::search_root_bisec_expectation(const FlowQuantity& left,
                                                                           const FlowQuantity& right,
                                                                           const FlowQuantity& production_quantity_X_p,
                                                                           const Price& unit_production_costs_n_c,
                                                                           const Price& n_min_p,
                                                                           const Price& precision_p) const {
    assert(left < right);  // check limits of interval
    // check if root is in interval

    FlowQuantity middle = (left + right) / 2.0;
    if (left + FlowQuantity(FlowQuantity::precision) >= right) {  // interval too narrow
        if (abs(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X_p + left, unit_production_costs_n_c, n_min_p))
            < abs(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X_p + right, unit_production_costs_n_c, n_min_p))) {
            return Flow(left, calc_additional_revenue_expectation(round(production_quantity_X_p + left), n_min_p));
        }
        return Flow(right, calc_additional_revenue_expectation(round(production_quantity_X_p + right), n_min_p));
    }
    // check if root is in interval
    assert(!same_sgn(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X_p + left, unit_production_costs_n_c, n_min_p),
                     goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X_p + right, unit_production_costs_n_c, n_min_p)));

    if (abs(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X_p + middle, unit_production_costs_n_c, n_min_p)) < precision_p) {
        return Flow(middle, calc_additional_revenue_expectation(production_quantity_X_p + middle, n_min_p));
    }
    if (abs(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X_p + left, unit_production_costs_n_c, n_min_p)) < precision_p) {
        return Flow(left, calc_additional_revenue_expectation(production_quantity_X_p + left, n_min_p));
    }
    if (abs(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X_p + right, unit_production_costs_n_c, n_min_p)) < precision_p) {
        return Flow(right, calc_additional_revenue_expectation(production_quantity_X_p + right, n_min_p));
    }
    if (!same_sgn(goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X_p + middle, unit_production_costs_n_c, n_min_p),
                  goal_fkt_marginal_costs_minus_marginal_revenue(production_quantity_X_p + right, unit_production_costs_n_c, n_min_p))) {
        return search_root_bisec_expectation(middle, right, production_quantity_X_p, unit_production_costs_n_c, n_min_p, precision_p);
    }
    return search_root_bisec_expectation(left, middle, production_quantity_X_p, unit_production_costs_n_c, n_min_p, precision_p);
}

#ifdef DEBUG
template<class ModelVariant>
void SalesManagerPrices<ModelVariant>::print_parameters() const {
    info(PRINT_ROW1("X", communicated_parameters_.production_X.get_quantity())
         << PRINT_ROW1("X_exp", communicated_parameters_.expected_production_X.get_quantity())
         << PRINT_ROW1("X_hat", communicated_parameters_.possible_production_X_hat) << PRINT_ROW1("  lambda", firm->forcing_lambda())
         << PRINT_ROW1("X_star", firm->initial_production_X_star().get_quantity())
         << PRINT_ROW1("lambda X_star", firm->forced_initial_production_quantity_lambda_X_star())
         << PRINT_ROW1("p", communicated_parameters_.production_X.get_quantity() / firm->initial_production_X_star().get_quantity())
         << PRINT_ROW1("p_exp", communicated_parameters_.expected_production_X.get_quantity() / firm->initial_production_X_star().get_quantity())
         << PRINT_ROW1("m_prod_cost(X)", calc_marginal_production_costs(communicated_parameters_.production_X.get_quantity(),
                                                                        communicated_parameters_.possible_production_X_hat.get_price() * (1 - tax_)))
         << PRINT_ROW1("expected m_prod_cost(X)", calc_marginal_production_costs(communicated_parameters_.expected_production_X.get_quantity(),
                                                                                 communicated_parameters_.possible_production_X_hat.get_price() * (1 - tax_)))
         << PRINT_ROW1("n_bar", communicated_parameters_.offer_price_n_bar) << PRINT_ROW1("n_c", communicated_parameters_.possible_production_X_hat.get_price())
         << PRINT_ROW1("  C", total_production_costs_C_));
}
#endif

#ifdef DEBUG
template<class ModelVariant>
void SalesManagerPrices<ModelVariant>::print_connections(
    typename std::vector<std::unique_ptr<BusinessConnection<ModelVariant>>>::const_iterator begin_equally_distributed,
    typename std::vector<std::unique_ptr<BusinessConnection<ModelVariant>>>::const_iterator end_equally_distributed) const {
#pragma omp critical(output)
    {
        std::cout << Acclimate::instance()->timeinfo() << ", " << std::string(*this) << ": supply distribution for " << business_connections.size()
                  << " outputs :\n";
        FlowQuantity sum = FlowQuantity(0.0);
        FlowQuantity initial_sum = FlowQuantity(0.0);
        for (const auto& bc : business_connections) {
            std::cout << "      " << std::string(*bc) << " :\n"
                      << PRINT_ROW1("n", bc->last_demand_request_D().get_price()) << PRINT_ROW1("D_r", bc->last_demand_request_D().get_quantity())
                      << PRINT_ROW1("D_star", bc->initial_flow_Z_star().get_quantity()) << PRINT_ROW1("Z_last", bc->last_shipment_Z(this).get_quantity())
                      << PRINT_ROW1("in_share", (bc->initial_flow_Z_star() / bc->buyer->storage->initial_input_flow_I_star())) << '\n';
            sum += bc->last_demand_request_D().get_quantity();
            initial_sum += bc->initial_flow_Z_star().get_quantity();
        }
        if (supply_distribution_scenario.connection_not_served_completely != business_connections.end()) {
            std::cout << "      not completely served: " << std::string(**supply_distribution_scenario.connection_not_served_completely) << '\n';
        } else {
            std::cout << "      all connections served completely\n";
        }
        if (begin_equally_distributed != business_connections.end()) {
            std::cout << "      first_equal_distribution: " << std::string(**begin_equally_distributed) << '\n';
        }
        if (end_equally_distributed != business_connections.end()) {
            std::cout << "      last_equal_distribution:  " << std::string(**end_equally_distributed) << '\n';
        }
        std::cout << "      Sums:\n" << PRINT_ROW1("sum D_r", sum) << PRINT_ROW1("sum_star D_r", initial_sum);
    }
}
#endif

INSTANTIATE_PRICES(SalesManagerPrices);
}  // namespace acclimate
