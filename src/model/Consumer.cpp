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

#include "model/Consumer.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "ModelRun.h"
#include "acclimate.h"
#include "autodiff.h"
#include "model/Model.h"
#include "model/PurchasingManager.h"
#include "model/Region.h"
#include "model/Storage.h"
#include "optimization.h"
#include "parameters.h"

static constexpr auto MAX_GRADIENT = 1e3;  // TODO: any need to modify?
static constexpr bool IGNORE_ROUNDOFFLIMITED = false;
static constexpr bool VERBOSE_CONSUMER = true;

namespace acclimate {
// TODO: in the long run abstract consumer with different implementations would be nice

Consumer::Consumer(id_t id_p,
                   Region* region_p,
                   FloatType inter_basket_substitution_coefficient_p,
                   std::vector<std::pair<std::vector<Sector*>, FloatType>> consumer_baskets_p)
    : EconomicAgent(std::move(id_p), region_p, EconomicAgent::type_t::CONSUMER),
      inter_basket_substitution_coefficient(std::move(inter_basket_substitution_coefficient_p)),
      consumer_baskets(std::move(consumer_baskets_p)) {
    inter_basket_substitution_exponent = (inter_basket_substitution_coefficient - 1) / inter_basket_substitution_coefficient;
    baskets_num = consumer_baskets.size();
    for (std::size_t r = 0; r < baskets_num; ++r) {
        basket_sectors.push_back(consumer_baskets[r].first);
        intra_basket_substitution_coefficient.push_back(consumer_baskets[r].second);
    }
}

void Consumer::initialize() {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    utilitarian = model()->parameters_writable().consumer_utilitarian;
    goods_num = input_storages.size();
    autodiffutility = {goods_num, 0.0};
    autodiff_basket_consumption_utility = {goods_num, 0.0};
    autodiff_consumption_utility = {goods_num, 0.0};
    var_optimizer_consumption = autodiff::Variable<FloatType>(0, goods_num, goods_num, 0.0);

    basket_share_factors.reserve(baskets_num);

    share_factors.reserve(goods_num);
    previous_consumption.reserve(goods_num);
    baseline_consumption.reserve(goods_num);
    // initialize previous consumption as starting values, calculate budget
    budget = FlowValue(0.0);
    not_spent_budget = FlowValue(0.0);
    for (std::size_t r = 0; r < goods_num; ++r) {
        const auto initial_consumption = input_storages[r]->initial_used_flow_U_star();
        previous_consumption.push_back(initial_consumption);
        desired_consumption.push_back(initial_consumption);
        budget += initial_consumption.get_value();
        baseline_consumption.push_back(initial_consumption);
    }
    // initialize share factors
    for (std::size_t r = 0; r < goods_num; ++r) {
        double budget_share = to_float(previous_consumption[r].get_value()) / to_float(budget);
        share_factors.push_back(budget_share);
    }
    // calculate share factors of baskets
    for (std::size_t basket = 0; basket < baskets_num; ++basket) {
        for (std::size_t good = 0; good < goods_num; ++good) {
            if (find(basket_sectors[basket].begin(), basket_sectors[basket].end(), model()->sectors[good + 1]) != basket_sectors[basket].end()) {
                basket_share_factors[basket] += share_factors[good];
            }
        }
        for (std::size_t good = 0; good < goods_num; ++good) {
            if (find(basket_sectors[basket].begin(), basket_sectors[basket].end(), model()->sectors[good + 1]) != basket_sectors[basket].end()) {
                share_factors[good] = share_factors[good] / basket_share_factors[basket];  // normalize nested share factors to 1
            }
            share_factors[good] = pow(share_factors[good], 1 / intra_basket_substitution_coefficient[basket]);  // already exponentiated for utility function
        }
        basket_share_factors[basket] =
            pow(basket_share_factors[basket], 1 / inter_basket_substitution_coefficient);  // already exponentiated for utility function
        intra_basket_substitution_exponent.push_back((intra_basket_substitution_coefficient[basket] - 1) / intra_basket_substitution_coefficient[basket]);
    }
    baseline_utility = CES_utility_function(baseline_consumption);  // baseline utility for scaling
}

// TODO: more sophisticated scaling than normalizing with baseline utility might improve optimization?!

// more complex nested utility function: goods are classified e.g. into necessary, relevant and other goods.

autodiff::Value<FloatType> Consumer::autodiff_nested_CES_utility_function(const autodiff::Variable<FloatType>& consumption_demands) {
    autodiff_consumption_utility = {goods_num, 0.0};
    for (std::size_t basket = 0; basket < baskets_num; ++basket) {
        autodiff_basket_consumption_utility = {goods_num, 0.0};
        for (std::size_t good = 0; good < goods_num; ++good) {
            if (find(basket_sectors[basket].begin(), basket_sectors[basket].end(), model()->sectors[good + 1]) != basket_sectors[basket].end()) {
                autodiff_basket_consumption_utility += pow(consumption_demands[good], intra_basket_substitution_exponent[basket]) * share_factors[good];
            }
        }
        autodiff_basket_consumption_utility = pow(autodiff_basket_consumption_utility, 1 / intra_basket_substitution_coefficient[basket]);
        autodiff_consumption_utility += pow(autodiff_basket_consumption_utility, inter_basket_substitution_exponent) * basket_share_factors[basket];
    }
    return autodiff_consumption_utility;
}

// definition of a nested CES utility function:
FloatType Consumer::CES_utility_function(const std::vector<FloatType>& consumption_demands) {
    consumption_utility = 0.0;
    for (std::size_t basket = 0; basket < baskets_num; ++basket) {
        basket_consumption_utility = 0;
        for (std::size_t good = 0; good < goods_num; ++good) {
            if (find(basket_sectors[basket].begin(), basket_sectors[basket].end(), model()->sectors[good + 1]) != basket_sectors[basket].end()) {
                basket_consumption_utility += pow(consumption_demands[good], intra_basket_substitution_exponent[basket]) * share_factors[good];
            }
        }
        basket_consumption_utility = pow(basket_consumption_utility, 1 / intra_basket_substitution_coefficient[basket]);
        consumption_utility += pow(basket_consumption_utility, inter_basket_substitution_exponent) * basket_share_factors[basket];
    }
    return consumption_utility;  // outer exponent not relevant for optimization (at least for sigma >1)
}
FloatType Consumer::CES_utility_function(const std::vector<Flow>& consumption_demands) {
    consumption_utility = 0.0;
    for (std::size_t basket = 0; basket < baskets_num; ++basket) {
        basket_consumption_utility = 0.0;
        for (std::size_t good = 0; good < goods_num; ++good) {
            if (find(basket_sectors[basket].begin(), basket_sectors[basket].end(), model()->sectors[good + 1]) != basket_sectors[basket].end()) {
                basket_consumption_utility +=
                    pow(to_float(consumption_demands[good].get_quantity()), intra_basket_substitution_exponent[basket]) * share_factors[good];
            }
        }
        basket_consumption_utility = pow(basket_consumption_utility, 1 / intra_basket_substitution_coefficient[basket]);
        consumption_utility += pow(basket_consumption_utility, inter_basket_substitution_exponent) * basket_share_factors[basket];
    }
    return consumption_utility;  // outer exponent not relevant for optimization (at least for sigma >1)
}

// TODO: more complex budget function might be useful, e.g. opportunity for saving etc.
FloatType Consumer::equality_constraint(const double* x, double* grad) { return inequality_constraint(x, grad); }

FloatType Consumer::inequality_constraint(const double* x, double* grad) {
    consumption_budget = to_float(budget + not_spent_budget);
    double scaled_budget = 1;
    for (std::size_t r = 0; r < goods_num; ++r) {
        assert(!std::isnan(x[r]));
        scaled_budget -= to_float(invert_scaling_double_to_quantity(x[r], r)) * to_float(consumption_prices[r]) / consumption_budget;
        if (grad != nullptr) {
            grad[r] = to_float(invert_scaling_double_to_quantity(x[r], r)) * to_float(consumption_prices[r]) / consumption_budget;
            if constexpr (options::OPTIMIZATION_WARNINGS) {
                assert(!std::isnan(grad[r]));
                if (grad[r] > MAX_GRADIENT) {
                    log::warning(this, ": large gradient of ", grad[r]);
                }
            }
        }
    }
    return -scaled_budget;  // since inequality constraint checks for <=0, we need to switch the sign
}

FloatType Consumer::max_objective(const double* x, double* grad) {
    for (std::size_t r = 0; r < goods_num; ++r) {
        assert(!std::isnan(x[r]));
        var_optimizer_consumption.value()[r] = to_float(invert_scaling_double_to_quantity(x[r], r));
    }
    autodiffutility = autodiff_nested_CES_utility_function(var_optimizer_consumption);
    if (grad != nullptr) {
        std::copy(std::begin(autodiffutility.derivative()), std::end(autodiffutility.derivative()), grad);
        for (std::size_t r = 0; r < goods_num; ++r) {
            grad[r] = grad[r] / baseline_utility;
        }
        if constexpr (options::OPTIMIZATION_WARNINGS) {
            for (std::size_t r = 0; r < goods_num; ++r) {
                assert(!std::isnan(grad[r]));
                if (grad[r] > MAX_GRADIENT) {
                    log::warning(this, ": large gradient of ", grad[r]);
                }
            }
        }
    }
    return autodiffutility.value() / baseline_utility;
}

void Consumer::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    if (utilitarian) {
        desired_consumption = utilitarian_consumption_optimization();
        utilitarian_consumption_execution(desired_consumption);
        local_optimal_utility =
            CES_utility_function(desired_consumption) / baseline_utility;  // just making sure the field has data, identical to utility in this case
    } else {
        // calculate local optimal utility consumption for comparison:
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, "local utilitarian consumption optimization:");
        }
        utilitarian_consumption = utilitarian_consumption_optimization();
        local_optimal_utility = CES_utility_function(utilitarian_consumption) / baseline_utility;
        // old consumer to be used for comparison
        desired_consumption.clear();
        desired_consumption.reserve(goods_num);
        previous_consumption.clear();
        previous_consumption.reserve(goods_num);
        for (const auto& is : input_storages) {
            Flow possible_used_flow_U_hat = is->get_possible_use_U_hat();  // Price(U_hat) = Price of used flow
            Price reservation_price;
            if (possible_used_flow_U_hat.get_quantity() > 0.0) {
                // we have to purchase with the average price of input and storage
                reservation_price = possible_used_flow_U_hat.get_price();
            } else {  // possible used flow is zero
                Price last_reservation_price = is->desired_used_flow_U_tilde(this).get_price();
                assert(!isnan(last_reservation_price));
                // price is calculated from last desired used flow
                reservation_price = last_reservation_price;
                model()->run()->event(EventType::NO_CONSUMPTION, this, nullptr, to_float(last_reservation_price));
            }
            assert(reservation_price > 0.0);

            const Flow desired_used_flow_U_tilde = Flow(round(is->initial_input_flow_I_star().get_quantity() * forcing_m
                                                              * pow(reservation_price / Price(1.0), is->parameters().consumption_price_elasticity)),
                                                        reservation_price);
            const Flow used_flow_U =
                Flow(FlowQuantity(std::min(desired_used_flow_U_tilde.get_quantity(), possible_used_flow_U_hat.get_quantity())), reservation_price);
            is->set_desired_used_flow_U_tilde(desired_used_flow_U_tilde);
            is->use_content_S(round(used_flow_U));
            region->add_consumption_flow_Y(round(used_flow_U));
            is->iterate_consumption_and_production();
            desired_consumption.push_back((used_flow_U));
            previous_consumption.emplace_back((used_flow_U.get_quantity()));
        }
    }
    utility = CES_utility_function(desired_consumption) / baseline_utility;
    log::info(this, "baseline relative utility:", utility);
}

std::vector<Flow> Consumer::utilitarian_consumption_optimization() {
    // get storage content of each good as maximal possible consumption and get prices from storage goods.
    // TODO: extend by alternative prices of most recent goods
    std::vector<Flow> possible_consumption;
    std::vector<Price> current_prices;
    consumption_prices.clear();
    consumption_prices.reserve(goods_num);
    starting_value_quantity = std::vector<FlowQuantity>(goods_num);
    scaling_quantity = std::vector<FlowQuantity>(goods_num);
    std::vector<FloatType> scaled_starting_value = std::vector<FloatType>(goods_num);
    std::vector<FloatType> maximum_affordable_consumption = std::vector<FloatType>(goods_num);

    for (std::size_t r = 0; r < goods_num; ++r) {
        possible_consumption.push_back(input_storages[r]->get_possible_use_U_hat());
        FlowQuantity possible_consumption_quantity = (possible_consumption[r].get_quantity());
        consumption_prices.push_back(possible_consumption[r].get_price());
        // consumption_prices.push_back(current_price);  // using this would be giving the consumer some "foresight" by optimizing using the current price of
        // new goods, not just the average price of storage goods
        // start optimization at previous consumption, guarantees stability in undisturbed baseline
        // while potentially speeding up optimization in case of small price changes
        auto starting_value_flow = (to_float(previous_consumption[r].get_quantity()) == 0.0) ? baseline_consumption[r] : previous_consumption[r];
        starting_value_quantity[r] = starting_value_flow.get_quantity();
        scaling_quantity[r] = baseline_consumption[r].get_quantity();
        starting_value_quantity[r] = std::min(starting_value_quantity[r], possible_consumption_quantity);
        // adjust if price changes make previous consumption to expensive - scaling with elasticity
        // TODO: one could try a more sophisticated use of price elasticity
        starting_value_quantity[r] =
            starting_value_quantity[r]
            * pow(to_float(consumption_prices[r]) / to_float(starting_value_flow.get_price()), input_storages[r]->parameters().consumption_price_elasticity);
        // calculate maximum affordable consumption based on budget and possible consumtpion quantity:
        if (to_float(starting_value_quantity[r]) <= 0.0) {
            maximum_affordable_consumption[r] = 0;
        } else {
            maximum_affordable_consumption[r] = std::min(1.0, to_float(budget) / (to_float(possible_consumption_quantity) * to_float(consumption_prices[r])))
                                                * (to_float(possible_consumption_quantity) / to_float(scaling_quantity[r]));
        }
        // scale starting value with scaling_value to use in optimization
        scaled_starting_value[r] = scale_quantity_to_double(starting_value_quantity[r], r);
    }
    // utility optimization
    // set parameters
    xtol_abs = std::vector(goods_num, FlowQuantity::precision);
    xtol_abs_global = std::vector(goods_num, FlowQuantity::precision * 100);
    optimizer_consumption = std::vector<double>(goods_num);  // use normalized variable for optimization to improve?! performance
    upper_bounds = std::vector<FloatType>(goods_num);
    lower_bounds =
        std::vector<FloatType>(goods_num, 0.00001);  // lower bound of exactly 0 is violated by Nlopt - bug?! - thus keeping minimum consumption level

    for (std::size_t r = 0; r < goods_num; ++r) {
        // scale xtol_abs
        xtol_abs[r] = scale_double_to_double(xtol_abs[r], r);
        xtol_abs_global[r] = scale_double_to_double(xtol_abs_global[r], r);
        ;
        // set bounds
        upper_bounds[r] = maximum_affordable_consumption[r];
        optimizer_consumption[r] = std::min(scaled_starting_value[r], upper_bounds[r]);
    }
    if constexpr (VERBOSE_CONSUMER) {
        log::info(this, "\"upper bounds\"");
        for (std::size_t r = 0; r < goods_num; ++r) {
            log::info(input_storages[r], "upper bound: ", upper_bounds[r]);
        }
    }

    // define local optimizer
    optimization::Optimization local_optimizer(static_cast<nlopt_algorithm>(model()->parameters().utility_optimization_algorithm),
                                               goods_num);  // TODO keep and only recreate when resize is needed

    // set parameters
    local_optimizer.xtol(xtol_abs);
    local_optimizer.maxeval(model()->parameters().optimization_maxiter);
    local_optimizer.maxtime(model()->parameters().optimization_timeout);

    if (model()->parameters().global_optimization) {
        // define  lagrangian optimizer to pass (in)equality constraints to global algorithm which cant use it directly:
        optimization::Optimization lagrangian_optimizer(static_cast<nlopt_algorithm>(model()->parameters().lagrangian_algorithm),
                                                        goods_num);  // TODO keep and only recreate when resize is needed
        if (model()->parameters().budget_inequality_constrained) {
            lagrangian_optimizer.add_inequality_constraint(this, FlowValue::precision);
        } else {
            lagrangian_optimizer.add_equality_constraint(this, FlowValue::precision);
        }
        lagrangian_optimizer.add_max_objective(this);
        lagrangian_optimizer.lower_bounds(lower_bounds);
        lagrangian_optimizer.upper_bounds(upper_bounds);
        lagrangian_optimizer.xtol(xtol_abs_global);
        lagrangian_optimizer.maxeval(model()->parameters().optimization_maxiter);
        lagrangian_optimizer.maxtime(model()->parameters().optimization_timeout);

        // define global optimizer to use random sampling MLSL algorithm as global search, before local optimization via local_optimizer:
        optimization::Optimization global_optimizer(static_cast<nlopt_algorithm>(model()->parameters().global_optimization_algorithm),
                                                    goods_num);  // TODO keep and only recreate when resize is needed

        global_optimizer.xtol(xtol_abs_global);
        global_optimizer.maxeval(model()->parameters().global_optimization_maxiter);
        global_optimizer.maxtime(model()->parameters().optimization_timeout);

        global_optimizer.set_local_algorithm(local_optimizer.get_optimizer());
        nlopt_set_population(global_optimizer.get_optimizer(),
                             0);  // one might adjust number of random sampling points per iteration (algorithm chooses if left at 0)

        // start combined global local optimizer optimizer
        lagrangian_optimizer.set_local_algorithm(global_optimizer.get_optimizer());
        consumption_optimize(lagrangian_optimizer);
    } else {
        if (model()->parameters().budget_inequality_constrained) {
            local_optimizer.add_inequality_constraint(this, FlowValue::precision);
        } else {
            local_optimizer.add_equality_constraint(this, FlowValue::precision);
        }
        local_optimizer.add_max_objective(this);
        local_optimizer.lower_bounds(lower_bounds);
        local_optimizer.upper_bounds(upper_bounds);
        consumption_optimize(local_optimizer);
    }

    // set consumption and previous consumption
    for (std::size_t r = 0; r < goods_num; ++r) {
        desired_consumption[r] = Flow(FlowQuantity(invert_scaling_double_to_quantity(optimizer_consumption[r], r)), consumption_prices[r]);
    }
    return desired_consumption;
}

void Consumer::consumption_optimize(optimization::Optimization& optimizer) {
    try {
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, "distribution before optimization");
            for (std::size_t r = 0; r < goods_num; ++r) {
                log::info(input_storages[r], "target consumption: ", optimizer_consumption[r]);
            }
        }
        const auto res = optimizer.optimize(optimizer_consumption);
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, "distribution after optimization");
            for (std::size_t r = 0; r < goods_num; ++r) {
                log::info(input_storages[r], "optimized consumption: ", optimizer_consumption[r]);
            }
        }

        if (!res && !optimizer.xtol_reached()) {
            if (optimizer.roundoff_limited()) {
                if constexpr (!IGNORE_ROUNDOFFLIMITED) {
                    if constexpr (options::DEBUGGING) {
                        debug_print_distribution();
                    }
                    model()->run()->event(EventType::OPTIMIZER_ROUNDOFF_LIMITED, this);
                    if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                        throw log::error(this, "optimization is roundoff limited (for ", goods_num, " consumption goods)");
                    } else {
                        log::warning(this, "optimization is roundoff limited (for ", goods_num, " consumption goods)");
                    }
                }
            } else if (optimizer.maxeval_reached()) {
                if constexpr (options::DEBUGGING) {
                    debug_print_distribution();
                }
                model()->run()->event(EventType::OPTIMIZER_MAXITER, this);
                if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                    throw log::error(this, "optimization reached maximum iterations (for ", goods_num, " consumption goods)");
                } else {
                    log::warning(this, "optimization reached maximum iterations (for ", goods_num, " consumption goods)");
                }
            } else if (optimizer.maxtime_reached()) {
                if constexpr (options::DEBUGGING) {
                    debug_print_distribution();
                }
                model()->run()->event(EventType::OPTIMIZER_TIMEOUT, this);
                if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                    throw log::error(this, "optimization timed out (for ", goods_num, " consumption goods)");
                } else {
                    log::warning(this, "optimization timed out (for ", goods_num, " consumption goods)");
                }
            } else {
                log::warning(this, "optimization finished with ", optimizer.last_result_description());
            }
        }
    } catch (const optimization::failure& ex) {
        if constexpr (options::DEBUGGING) {
            debug_print_distribution();
        }
        throw log::error(this, "optimization failed, ", ex.what(), " (for ", goods_num, " consumption goods)");
    }
}

void Consumer::utilitarian_consumption_execution(std::vector<Flow> desired_consumption) {
    // initialize available budget
    not_spent_budget += budget;
    previous_consumption.clear();
    previous_consumption.reserve(goods_num);
    for (std::size_t r = 0; r < goods_num; ++r) {
        // withdraw consumption from storage
        input_storages[r]->set_desired_used_flow_U_tilde((desired_consumption[r]));
        input_storages[r]->use_content_S((desired_consumption[r]));
        region->add_consumption_flow_Y((desired_consumption[r]));
        // consume goods
        input_storages[r]->iterate_consumption_and_production();
        // adjust previous consumption
        previous_consumption[r] = (desired_consumption[r]);
        // adjust non-spent budget
        not_spent_budget -= desired_consumption[r].get_value();
    }
    not_spent_budget = FlowValue(0.0);  // introduces instability, therefore "disable" not_spent_budget TODO: fix this
}

void Consumer::iterate_expectation() { debug::assertstep(this, IterationStep::EXPECTATION); }

void Consumer::iterate_purchase() {
    debug::assertstep(this, IterationStep::PURCHASE);
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_purchase();
    }
}

void Consumer::iterate_investment() {
    // debug::assertstep(this, IterationStep::INVESTMENT);
    // for (const auto& is : input_storages) {
    //     is->purchasing_manager->iterate_investment();
    // }
}

void Consumer::debug_print_details() const {
    // TODO: adjust this to give info on U(x), budget, starting values, etc.
    if constexpr (options::DEBUGGING) {
        log::info(this);
        for (const auto& is : input_storages) {
            is->purchasing_manager->debug_print_details();
        }
    }
}

template<typename T1, typename T2>
static void print_row(T1 a, T2 b) {
    std::cout << "      " << std::setw(24) << a << " = " << std::setw(14) << b << '\n';
}

void Consumer::debug_print_distribution() {
    if constexpr (options::DEBUGGING) {
#pragma omp critical(output)
        {
            std::vector<double> grad_constraint(goods_num);
            std::vector<double> grad_objective(goods_num);

            std::cout << model()->run()->timeinfo() << ", " << name() << ": demand distribution for " << goods_num << " inputs :\n";
            print_row("inequality value", inequality_constraint(&optimizer_consumption[0], &grad_constraint[0]));
            print_row("objective value", max_objective(&optimizer_consumption[0], &grad_objective[0]));
            print_row("current utility", utility / baseline_utility);
            print_row("current total budget", budget);
            print_row("substitution coefficient", inter_basket_substitution_coefficient);
            std::cout << '\n';

            for (std::size_t r = 0; r < goods_num; ++r) {
                std::cout << "    " << input_storages[r]->name() << " :\n";
                print_row("grad (constraint)", grad_constraint[r]);
                print_row("grad (objective)", grad_objective[r]);
                print_row("share factor", share_factors[r]);
                print_row("consumption quantity", invert_scaling_double_to_quantity(optimizer_consumption[r], r));
                print_row("starting cons. quantity", starting_value_quantity[r]);
                print_row("consumption price", consumption_prices[r]);
                print_row("consumption value", invert_scaling_double_to_quantity(optimizer_consumption[r], r) * consumption_prices[r]);
                std::cout << '\n';
            }
        }
    }
}

FlowQuantity Consumer::invert_scaling_double_to_quantity(double scaled_value, int scaling_index) const {
    return scaled_value * scaling_quantity[scaling_index];
}
double Consumer::scale_quantity_to_double(FlowQuantity quantity, int scaling_index) const { return to_float(quantity / scaling_quantity[scaling_index]); }
double Consumer::scale_double_to_double(double not_scaled_double, int scaling_index) const {
    return not_scaled_double / to_float(scaling_quantity[scaling_index]);
}
}  // namespace acclimate
