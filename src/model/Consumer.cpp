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
                   std::vector<std::vector<int>> consumer_baskets_p,
                   std::vector<FloatType> intra_basket_substitution_coefficients_p)
    : EconomicAgent(std::move(id_p), region_p, EconomicAgent::type_t::CONSUMER) {
    inter_basket_substitution_coefficient = inter_basket_substitution_coefficient_p;
    inter_basket_substitution_exponent = (inter_basket_substitution_coefficient - 1) / inter_basket_substitution_coefficient;
    goods_basket = consumer_baskets_p;
    baskets_num = goods_basket.size();
    intra_basket_substitution_coefficient.reserve(baskets_num);
    intra_basket_substitution_exponent = std::vector<FloatType>(baskets_num);
    intra_basket_substitution_coefficient = intra_basket_substitution_coefficients_p;
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
    consumption_vector = std::vector<FloatType>(goods_num);
    // initialize previous consumption as starting values, calculate budget
    budget = FlowValue(0.0);
    not_spent_budget = FlowValue(0.0);
    for (std::size_t r = 0; r < goods_num; ++r) {
        const auto initial_consumption = input_storages[r]->initial_used_flow_U_star();
        previous_consumption.push_back(initial_consumption);
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
            if (find(goods_basket[basket].begin(), goods_basket[basket].end(), good) != goods_basket[basket].end()) {
                basket_share_factors[basket] += share_factors[good];
            }
        }
        for (std::size_t good = 0; good < goods_num; ++good) {
            if (find(goods_basket[basket].begin(), goods_basket[basket].end(), good) != goods_basket[basket].end()) {
                share_factors[good] = share_factors[good] / basket_share_factors[basket];  // normalize nested share factors to 1
            }
            share_factors[good] = pow(share_factors[good], 1 / intra_basket_substitution_coefficient[basket]);  // already exponentiated for utility function
        }
        basket_share_factors[basket] =
            pow(basket_share_factors[basket], 1 / inter_basket_substitution_coefficient);  // already exponentiated for utility function
        intra_basket_substitution_exponent[basket] = (intra_basket_substitution_coefficient[basket] - 1) / intra_basket_substitution_coefficient[basket];
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
            if (find(goods_basket[basket].begin(), goods_basket[basket].end(), good) != goods_basket[basket].end()) {
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
            if (find(goods_basket[basket].begin(), goods_basket[basket].end(), good) != goods_basket[basket].end()) {
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
            if (find(goods_basket[basket].begin(), goods_basket[basket].end(), good) != goods_basket[basket].end()) {
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
FloatType Consumer::inequality_constraint(const double* x, double* grad) {
    consumption_budget = to_float(budget + not_spent_budget);
    for (std::size_t r = 0; r < goods_num; ++r) {
        assert(!std::isnan(x[r]));
        consumption_vector[r] = unscaled_demand(x[r], r);
        consumption_budget -= consumption_vector[r] * to_float(consumption_prices[r]);
    }
    grad = fill_gradient(grad);
    return consumption_budget * -1;  // since inequality constraint checks for <=0, we need to switch the sign
}

FloatType Consumer::max_objective(const double* x, double* grad) {
    for (std::size_t r = 0; r < goods_num; ++r) {
        assert(!std::isnan(x[r]));
        consumption_vector[r] = unscaled_demand(x[r], r);
    }
    var_optimizer_consumption = consumption_vector;
    autodiffutility = autodiff_nested_CES_utility_function(var_optimizer_consumption);
    grad = fill_gradient(grad);
    return autodiffutility.value();
}

double* Consumer::fill_gradient(double* grad) {
    if (grad != nullptr) {
        std::copy(std::begin(autodiffutility.derivative()), std::end(autodiffutility.derivative()), grad);
        if constexpr (options::OPTIMIZATION_WARNINGS) {
            for (std::size_t r = 0; r < goods_num; ++r) {
                if (grad[r] > MAX_GRADIENT) {
                    log::warning(this, ": large gradient of ", grad[r]);
                }
            }
        }
    }
    return grad;
}

void Consumer::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    desired_consumption = std::vector<FloatType>(goods_num);
    if (utilitarian) {
        utilitarian_consumption_optimization();
        utilitarian_consumption_execution(desired_consumption);
        local_optimal_utility =
            CES_utility_function(desired_consumption) / baseline_utility;  // just making sure the field has data, identical to utility in this case
    } else {
        // calculate local optimal utility consumption for comparison:
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, "local utilitarian  consumption optimization:");
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
            desired_consumption.push_back(to_float(used_flow_U.get_quantity()));
            previous_consumption.emplace_back((used_flow_U.get_quantity()));
        }
    }
    utility = CES_utility_function(desired_consumption) / baseline_utility;  // utility calculated to compare
}

std::vector<FloatType> Consumer::utilitarian_consumption_optimization() {
    // get storage content of each good as maximal possible consumption and get prices from storage goods.
    // TODO: extend by alternative prices of most recent goods
    std::vector<Flow> possible_consumption;
    std::vector<Price> current_prices;
    consumption_prices.clear();
    consumption_prices.reserve(goods_num);
    std::vector<FloatType> consumption_scaling = std::vector<FloatType>(goods_num);
    std::vector<FloatType> single_good_maximum_consumption = std::vector<FloatType>(goods_num);

    for (std::size_t r = 0; r < goods_num; ++r) {
        possible_consumption.push_back(input_storages[r]->get_possible_use_U_hat());
        FlowQuantity possible_consumption_quantity = (possible_consumption[r].get_quantity());
        consumption_prices.push_back(possible_consumption[r].get_price());
        // consumption_prices.push_back(current_price);  // using this would be giving the consumer some "foresight" by optimizing using the current price of
        // new goods, not just the average price of storage goods
        // start optimization at previous consumption, guarantees stability in undisturbed baseline
        // while potentially speeding up optimization in case of small price changes
        auto desired_consumption_flow = (to_float(baseline_consumption[r].get_quantity()) == 0.0) ? baseline_consumption[r] : baseline_consumption[r];
        desired_consumption[r] = (to_float(desired_consumption_flow.get_quantity()));
        // adjust if price changes make previous consumption to expensive - scaling with elasticity
        consumption_scaling[r] =
            pow(to_float(consumption_prices[r]) / to_float(desired_consumption_flow.get_price()), input_storages[r]->parameters().consumption_price_elasticity);
        desired_consumption[r] = std::min(desired_consumption[r], to_float(possible_consumption_quantity));
        if (desired_consumption[r] == 0) {
            single_good_maximum_consumption[r] = 0;
        } else {
            single_good_maximum_consumption[r] = std::min(1.0, to_float(budget) / (to_float(possible_consumption_quantity) * to_float(consumption_prices[r])))
                                                 * ((to_float(possible_consumption_quantity)) / (desired_consumption[r]));
        }

        // TODO: one could try a more sophisticated use of price elasticity
    }
    // utility optimization
    // set parameters
    xtol_abs = std::vector(goods_num, FlowQuantity::precision);
    xtol_abs_global = std::vector(goods_num, 0.1);
    optimizer_consumption = std::vector<double>(goods_num);  // use normalized variable for optimization to improve?! performance
    upper_bounds = std::vector<FloatType>(goods_num);
    lower_bounds =
        std::vector<FloatType>(goods_num, 0.00000001);  // lower bound of exactly 0 is violated by Nlopt - bug?! - thus keeping minimum consumption level

    for (std::size_t r = 0; r < goods_num; ++r) {
        // set bounds
        upper_bounds[r] = single_good_maximum_consumption[r];
        optimizer_consumption[r] = std::min(consumption_scaling[r], upper_bounds[r]);
    }
    if constexpr (VERBOSE_CONSUMER) {
        log::info(this, "\"upper bounds\"");
        for (std::size_t r = 0; r < goods_num; ++r) {
            log::info(this, "sector:", model()->sectors[r + 1]->name(), "\t upper bound:", upper_bounds[r]);
        }
    }


    // define local optimizer
    optimization::Optimization local_optimizer(static_cast<nlopt_algorithm>(model()->parameters().utility_optimization_algorithm),
                                               goods_num);  // TODO keep and only recreate when resize is needed

    // set parameters

    local_optimizer.add_inequality_constraint(this, FlowValue::precision);
    local_optimizer.add_max_objective(this);
    local_optimizer.xtol(xtol_abs);
    local_optimizer.lower_bounds(lower_bounds);
    local_optimizer.upper_bounds(upper_bounds);
    local_optimizer.maxeval(model()->parameters().optimization_maxiter);
    local_optimizer.maxtime(model()->parameters().optimization_timeout);

    if (model()->parameters().global_optimization == true) {
        // define  lagrangian optimizer to pass (in)equality constraints to global algorithm which cant use it directly:
        optimization::Optimization lagrangian_optimizer(static_cast<nlopt_algorithm>(model()->parameters().lagrangian_algorithm),
                                                        goods_num);  // TODO keep and only recreate when resize is needed
        lagrangian_optimizer.add_inequality_constraint(this, FlowValue::precision);
        lagrangian_optimizer.add_max_objective(this);
        lagrangian_optimizer.xtol(xtol_abs_global);
        lagrangian_optimizer.lower_bounds(lower_bounds);
        lagrangian_optimizer.upper_bounds(upper_bounds);
        lagrangian_optimizer.maxeval(model()->parameters().optimization_maxiter);
        lagrangian_optimizer.maxtime(model()->parameters().optimization_timeout);

        // define global optimizer to use random sampling MLSL algorithm as global search, before local optimization via local_optimizer:
        optimization::Optimization global_optimizer(static_cast<nlopt_algorithm>(model()->parameters().global_optimization_algorithm),
                                                    goods_num);  // TODO keep and only recreate when resize is needed

        global_optimizer.xtol(xtol_abs_global);
        global_optimizer.maxeval(model()->parameters().global_optimization_maxiter);
        global_optimizer.lower_bounds(lower_bounds);
        global_optimizer.upper_bounds(upper_bounds);
        global_optimizer.maxtime(model()->parameters().optimization_timeout);

        global_optimizer.set_local_algorithm(local_optimizer.get_optimizer());
        nlopt_set_population(global_optimizer.get_optimizer(),
                             0);  // one might adjust number of random sampling points per iteration (algorithm chooses if left at 0)

        // start combined global local optimizer optimizer
        lagrangian_optimizer.set_local_algorithm(global_optimizer.get_optimizer());

        consumption_optimize(lagrangian_optimizer);
    } else {
        consumption_optimize(local_optimizer);
    }

    // set consumption and previous consumption
    for (std::size_t r = 0; r < goods_num; ++r) {
        Flow desired_consumption_flow = (Flow(FlowQuantity(unscaled_demand(optimizer_consumption[r], r)), consumption_prices[r]));
        desired_consumption[r] = (to_float(desired_consumption_flow.get_quantity()));
    }
    return desired_consumption;
}

void Consumer::consumption_optimize(optimization::Optimization& optimizer) {
    try {
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, "distribution before optimization");
            for (std::size_t r = 0; r < goods_num; ++r) {
                log::info(this, "sector:", model()->sectors[r + 1]->name(), "\t target consumption:", optimizer_consumption[r]);
            }
        }
        const auto res = optimizer.optimize(optimizer_consumption);
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, "distribution after optimization");
            for (std::size_t r = 0; r < goods_num; ++r) {
                log::info(this, "sector:", model()->sectors[r + 1]->name(), "\t optimized consumption:", optimizer_consumption[r]);
            }
        }
        if (!res && !optimizer.xtol_reached()) {
            if (optimizer.roundoff_limited()) {
                if constexpr (!IGNORE_ROUNDOFFLIMITED) {
                    if constexpr (options::DEBUGGING) {
                        debug_print_distribution(optimizer_consumption);
                    }
                    model()->run()->event(EventType::OPTIMIZER_ROUNDOFF_LIMITED, this);
                    if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                        throw log::error(this, "optimization is roundoff limited (for ", goods_num, " consumption goods)");
                    } else {
                        log::warning(this, "optimization is roundoff limited (for ", goods_num, " consumption goods)");
                    }
                }
            }
        } else if (optimizer.maxtime_reached()) {
            if constexpr (options::DEBUGGING) {
                debug_print_distribution(optimizer_consumption);
            }
            model()->run()->event(EventType::OPTIMIZER_TIMEOUT, this);
            if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                throw log::error(this, "optimization timed out (for ", goods_num, " inputs)");
            } else {
                log::warning(this, "optimization timed out (for ", goods_num, " inputs)");
            }

        } else {
            log::warning(this, "optimization finished with ", optimizer.last_result_description());
        }
    } catch (const optimization::failure& ex) {
        if constexpr (options::DEBUGGING) {
            debug_print_distribution(optimizer_consumption);  // TODO: change to vector of flows?
        }
        // TODO maxiter limit is ok, the rest fatal
        if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
            throw log::error(this, "optimization failed, ", ex.what(), " (for ", goods_num, " inputs)");
        } else {
            log::warning(this, "optimization failed, ", ex.what(), " (for ", goods_num, " inputs)");
        }
    }
}

void Consumer::utilitarian_consumption_execution(std::vector<FloatType> requested_consumption) {
    // initialize available budget
    not_spent_budget += budget;
    previous_consumption.clear();
    previous_consumption.reserve(goods_num);
    for (std::size_t r = 0; r < goods_num; ++r) {
        // withdraw consumption from storage
        Flow desired_consumption_flow = (Flow(FlowQuantity(requested_consumption[r]), consumption_prices[r]));
        input_storages[r]->set_desired_used_flow_U_tilde((desired_consumption_flow));
        input_storages[r]->use_content_S((desired_consumption_flow));
        region->add_consumption_flow_Y((desired_consumption_flow));
        // consume goods
        input_storages[r]->iterate_consumption_and_production();
        // adjust previous consumption
        previous_consumption[r] = (desired_consumption_flow);
        // adjust non-spent budget
        not_spent_budget -= desired_consumption_flow.get_value();
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
    std::cout << "      " << std::setw(14) << a << " = " << std::setw(14) << b << '\n';
}

template<typename T1, typename T2, typename T3>
static void print_row(T1 a, T2 b, T3 c) {
    std::cout << "      " << std::setw(14) << a << " = " << std::setw(14) << b << " (" << c << ")\n";
}

void Consumer::debug_print_distribution(const std::vector<double>& consumption_demands) const {
    if constexpr (options::DEBUGGING) {
#pragma omp critical(output)
        {
            std::cout << model()->run()->timeinfo() << ", " << name() << ": demand distribution for " << goods_num << " inputs :\n";
            std::vector<FloatType> grad(goods_num);
            // max_objective(&consumption_demands[0], &grad[0]);
            for (std::size_t r = 0; r < goods_num; ++r) {
                std::cout << "      " << name() << " :\n";
                print_row("grad", grad[r]);
                print_row("share factor", share_factors[r]);
                print_row("substitution coefficient", inter_basket_substitution_coefficient);
                print_row("consumption request", unscaled_demand(consumption_demands[r], r));
                print_row("starting value consumption request", desired_consumption[r]);
                print_row("consumption price", consumption_prices[r]);
                print_row("current utility", utility / baseline_utility);
                print_row("current total budget", budget);
                print_row("current spending for this good", consumption_demands[r] * desired_consumption[r] * consumption_prices[r]);

                std::cout << '\n';
            }
        }
    }
}
FloatType Consumer::unscaled_demand(FloatType scaling_factor, int scaling_index) const { return scaling_factor * desired_consumption[scaling_index]; }

}  // namespace acclimate
