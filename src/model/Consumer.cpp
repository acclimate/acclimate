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
                   std::vector<std::pair<std::vector<Sector*>, FloatType>> consumer_baskets_p,
                   bool utilitarian_p)
    : EconomicAgent(std::move(id_p), region_p, EconomicAgent::type_t::CONSUMER), consumer_baskets(std::move(consumer_baskets_p)) {
    inter_basket_substitution_coefficient = inter_basket_substitution_coefficient_p;
    inter_basket_substitution_exponent = (inter_basket_substitution_coefficient - 1) / inter_basket_substitution_coefficient;
    utilitarian = utilitarian_p;
}
/**
 * initializes parameters of utility function depending on current input storages and
 * fields used in optimization methods to improve performance
 */
void Consumer::initialize() {
    debug::assertstep(this, IterationStep::INITIALIZATION);

    autodiffutility = {input_storages.size(), 0.0};
    autodiff_basket_consumption_utility = {input_storages.size(), 0.0};
    autodiff_consumption_utility = {input_storages.size(), 0.0};

    var_optimizer_consumption = autodiff::Variable<FloatType>(0, input_storages.size(), input_storages.size(), 0.0);

    share_factors = std::vector<FloatType>(input_storages.size());
    exponent_share_factors = std::vector<FloatType>(input_storages.size());
    previous_consumption.reserve(input_storages.size());
    baseline_consumption.reserve(input_storages.size());

    consumption_budget = FlowValue(0.0);
    not_spent_budget = FlowValue(0.0);

    for (auto& input_storage : input_storages) {
        previous_consumption.push_back(input_storage->initial_used_flow_U_star());
        baseline_consumption.push_back(input_storage->initial_used_flow_U_star());
        consumption_budget += input_storage->initial_used_flow_U_star().get_value();
    }

    if constexpr (VERBOSE_CONSUMER) {
        log::info("budget: ", consumption_budget);
    }

    intra_basket_substitution_coefficient = std::vector<FloatType>(consumer_baskets.size(), 0);
    intra_basket_substitution_exponent = std::vector<FloatType>(consumer_baskets.size(), 0);
    basket_share_factors = std::vector<FloatType>(consumer_baskets.size(), 0);
    exponent_basket_share_factors = std::vector<FloatType>(consumer_baskets.size(), 0);
    consumer_basket_indizes = std::vector<std::vector<int>>(consumer_baskets.size());
    for (int basket = 0; basket < int(consumer_baskets.size()); ++basket) {
        intra_basket_substitution_coefficient[basket] = (consumer_baskets[basket].second);
        intra_basket_substitution_exponent[basket] = (intra_basket_substitution_coefficient[basket] - 1) / intra_basket_substitution_coefficient[basket];
        for (auto& sector : consumer_baskets[basket].first) {
            for (auto& i_storage : input_storages)
                if (i_storage->sector == sector) {
                    basket_share_factors[basket] += to_float(i_storage->initial_used_flow_U_star().get_value()) / to_float(consumption_budget);
                }
        }
    }
    // edge case of no consumption in this basket - remove basket from optimization
    auto original_basket_number = consumer_baskets.size();
    log::info(this, "basketnumber: ", original_basket_number);
    for (int basket = original_basket_number; basket >= 0; basket--) {
        if (basket_share_factors[basket] == 0) {
            log::info(this, "basket: ", basket, " removed");
            consumer_baskets.erase(std::remove(consumer_baskets.begin(), consumer_baskets.end(), consumer_baskets[basket]), consumer_baskets.end());
        }
    }
    auto consumption_in_relevant_baskets = FlowValue(0.0);
    for (int basket = 0; basket < int(consumer_baskets.size()); ++basket) {
        for (auto& sector : consumer_baskets[basket].first) {
            for (auto& i_storage : input_storages)
                if (i_storage->sector == sector) {
                    consumption_in_relevant_baskets += i_storage->initial_used_flow_U_star().get_value();
                }
        }
    }

    std::vector<Sector*> all_relevant_sectors;
    for (int basket = 0; basket < int(consumer_baskets.size()); ++basket) {
        for (auto& sector : consumer_baskets[basket].first) {
            for (auto& i_storage : input_storages)
                if (i_storage->sector == sector) {
                    basket_share_factors[basket] = basket_share_factors[basket] * to_float(consumption_budget / consumption_in_relevant_baskets);
                    all_relevant_sectors.push_back(sector);
                }
        }
    }

    for (auto& input_storage : input_storages) {
        if (std::count(all_relevant_sectors.begin(), all_relevant_sectors.end(), input_storage->sector)) {
            share_factors[input_storage->id.index()] = to_float(input_storage->initial_used_flow_U_star().get_value() / consumption_budget)
                                                       * to_float(consumption_budget / consumption_in_relevant_baskets);
            previous_consumption[input_storage->id.index()] =
                input_storage->initial_used_flow_U_star();  // overwrite to get proper ordering with regard to input_storage[index].
            // workaround since no constructor for std::vector<Flow> available
            baseline_consumption[input_storage->id.index()] = input_storage->initial_used_flow_U_star();
        } else {
            share_factors[input_storage->id.index()] = 0;
            previous_consumption[input_storage->id.index()] = Flow(0);
            baseline_consumption[input_storage->id.index()] = Flow(0);
            log::info(this, "sector: ", input_storage->sector->id, " set to 0");
        }
    }

    for (int basket = 0; basket < int(consumer_baskets.size()); ++basket) {
        for (auto& sector : consumer_baskets[basket].first) {
            for (auto& i_storage : input_storages)
                if (i_storage->sector == sector) {
                    consumer_basket_indizes[basket].push_back(i_storage->id.index());
                    share_factors[i_storage->id.index()] =
                        share_factors[i_storage->id.index()] / basket_share_factors[basket];  // normalize share factors of a basket to 1

                    exponent_share_factors[i_storage->id.index()] = std::pow(
                        share_factors[i_storage->id.index()], 1 / intra_basket_substitution_coefficient[basket]);  // already with exponent for utility function
                }
        }

        exponent_basket_share_factors[basket] =
            std::pow(basket_share_factors[basket], 1 / inter_basket_substitution_coefficient);  // already with exponent for utility function
    }
}

// TODO: more sophisticated scaling than normalizing with baseline utility might improve optimization?!
/**
 * nested CES utility function using automatic differentiation to enable fast gradient calculation
 * @param consumption vector of consumed quantity of each good
 * @return auto-differentiable value of the utility function
 */
autodiff::Value<FloatType> Consumer::autodiff_nested_CES_utility_function(const autodiff::Variable<FloatType>& baseline_relative_consumption) {
    autodiff_consumption_utility.reset();  // reset to 0 without new call
    for (int basket = 0; basket < int(consumer_baskets.size()); ++basket) {
        autodiff_basket_consumption_utility.reset();
        for (auto& index : consumer_basket_indizes[basket]) {
            auto consumption_quantity = baseline_relative_consumption[index] * share_factors[index];
            autodiff_basket_consumption_utility += std::pow(consumption_quantity, intra_basket_substitution_exponent[basket]) * exponent_share_factors[index];
        }
        autodiff_basket_consumption_utility =
            std::pow(autodiff_basket_consumption_utility, 1 / intra_basket_substitution_exponent[basket]) * basket_share_factors[basket];

        autodiff_consumption_utility +=
            std::pow(autodiff_basket_consumption_utility, inter_basket_substitution_exponent) * exponent_basket_share_factors[basket];
    }
    autodiff_consumption_utility = std::pow(autodiff_consumption_utility, 1 / inter_basket_substitution_exponent);

    return autodiff_consumption_utility;
}

// TODO: more complex budget function might be useful, e.g. opportunity for saving etc.
/**
 * if budget should be spent always, equality constrained can be used instead of inequality constraint
 * @param x NLOpt convention of a c-style vector of consumption
 * @param grad gradient vector
 * @return value of the constraint, which is targeted to be =0 by NLOpt optimizers
 */
FloatType Consumer::equality_constraint(const double* x, double* grad) { return inequality_constraint(x, grad); }
/**
 * inequality constraint should be sufficient for utility optimization, since more spending -> more consumption should be induced by the objective anyways
 * @param x NLOpt convention of a c-style vector of consumption
 * @param grad gradient vector
 * @return value of the constraint, which is targeted to be <=0 by NLOpt optimizers
 */
FloatType Consumer::inequality_constraint(const double* x, double* grad) {
    double scaled_budget = (consumption_budget + not_spent_budget) / consumption_budget;
    for (auto& input_storage : input_storages) {
        int index = input_storage->id.index();
        assert(!std::isnan(x[index]));
        double price;
        if (model()->parameters().elastic_budget) {
            price = x[index] * std::pow(to_float(consumption_prices[index]), -1 * input_storage->parameters().consumption_price_elasticity);
        } else {
            price = x[index] * to_float(consumption_prices[index]);
        }
        scaled_budget -= invert_scaling_double_to_double(x[index], baseline_consumption[index].get_quantity()) * price / to_float(consumption_budget);
        if (grad != nullptr) {
            grad[index] = invert_scaling_double_to_double(x[index], baseline_consumption[index].get_quantity()) * price / to_float(consumption_budget);
            if constexpr (options::OPTIMIZATION_WARNINGS) {
                assert(!std::isnan(grad[index]));
                if (grad[index] > MAX_GRADIENT) {
                    log::warning(this, ": large gradient of ", grad[index]);
                }
            }
        }
    }
    return -scaled_budget;  // since inequality constraint checks for <=0, we need to switch the sign
}
/**
 * objective function for utility maximisation, using automatic-differentiable utility function
 * @param x NLOpt convention of a c-style vector of consumption
 * @param grad gradient vector
 * @return value of the objective function, which is maximized
 */
FloatType Consumer::max_objective(const double* x, double* grad) {
    for (auto& input_storage : input_storages) {
        int index = input_storage->id.index();
        assert(!std::isnan(x[index]));
        var_optimizer_consumption.value()[index] = x[index];
    }
    autodiffutility = autodiff_nested_CES_utility_function(var_optimizer_consumption);
    if (grad != nullptr) {
        std::copy(std::begin(autodiffutility.derivative()), std::end(autodiffutility.derivative()), grad);
        if constexpr (options::OPTIMIZATION_WARNINGS) {
            for (auto& input_storage : input_storages) {
                int index = input_storage->id.index();
                assert(!std::isnan(grad[index]));
                if (grad[index] > MAX_GRADIENT) {
                    log::warning(this, ": large gradient of ", grad[index]);
                }
            }
        }
    }
    return autodiffutility.value();
}

void Consumer::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    std::vector<Flow> consumption;
    consumption.reserve(input_storages.size());
    if (utilitarian) {
        auto optimization_result = utilitarian_consumption_optimization();
        consumption = optimization_result.first;

        consume_optimisation_result(consumption);
        local_optimal_utility = optimization_result.second;  // just making sure the field has data, identical to utility in this case
    } else {
        // calculate local optimal utility consumption for comparison:
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, "local utilitarian consumption optimization:");
        }
        local_optimal_utility = utilitarian_consumption_optimization().second;
        // old consumer to be used for comparison
        previous_consumption.clear();
        previous_consumption.reserve(input_storages.size());
        consumption.clear();
        consumption.reserve(input_storages.size());
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
                                                              * std::pow(reservation_price / Price(1.0), is->parameters().consumption_price_elasticity)),
                                                        reservation_price);
            const Flow used_flow_U =
                Flow(FlowQuantity(std::min(desired_used_flow_U_tilde.get_quantity(), possible_used_flow_U_hat.get_quantity())), reservation_price);
            is->set_desired_used_flow_U_tilde(desired_used_flow_U_tilde);
            is->use_content_S(round(used_flow_U));
            region->add_consumption_flow_Y(round(used_flow_U));
            is->iterate_consumption_and_production();
            consumption.push_back((used_flow_U));
            previous_consumption.emplace_back((used_flow_U.get_quantity()));
        }
    }
    utility = local_optimal_utility;  // TODO: determine whether this separation should be maintained
    log::info(this, "utility:", utility);
}

/**
 * NLOpt based optimization of consumption with respect to utility function
 * @return vector of utility maximising consumption flows
 */
std::pair<std::vector<Flow>, FloatType> Consumer::utilitarian_consumption_optimization() {
    std::vector<Price> current_prices;
    std::vector<FlowQuantity> starting_value_quantity(input_storages.size());
    std::vector<FloatType> scaled_starting_value = std::vector<FloatType>(input_storages.size());

    // optimization parameters
    std::vector<FloatType> xtol_abs(input_storages.size(), FlowQuantity::precision * model()->parameters().utility_optimization_precision_adjustment);
    std::vector<FloatType> xtol_abs_global(input_storages.size(),
                                           FlowQuantity::precision * model()->parameters().global_utility_optimization_precision_adjustment);
    std::vector<FloatType> lower_bounds(input_storages.size());
    std::vector<FloatType> upper_bounds = std::vector<FloatType>(input_storages.size());

    FloatType optimized_utility;

    // global fields to improve performance of optimization
    consumption_prices.clear();
    consumption_prices = std::vector<Price>(input_storages.size());

    for (auto& input_storage : input_storages) {
        int index = input_storage->id.index();
        FlowQuantity possible_consumption_quantity = (input_storage->get_possible_use_U_hat().get_quantity());
        consumption_prices[index] = input_storage->get_possible_use_U_hat().get_price();
        // TODO: extend by alternative prices of most recent goods, e.g.
        // consumption_prices.push_back(input_storage->current_input_flow_I().get_price());// using this would be giving the consumer some "foresight"
        /*
        start optimization at previous consumption, guarantees stability in undisturbed baseline
        while potentially speeding up optimization in case of small price changes*/
        Flow starting_value_flow = (to_float(previous_consumption[index].get_quantity()) == 0.0) ? baseline_consumption[index] : previous_consumption[index];
        starting_value_quantity[index] = std::min(starting_value_flow.get_quantity(), possible_consumption_quantity);
        // adjust if price changes make previous consumption to expensive - scaling with elasticity
        // TODO: one could try a more sophisticated use of price elasticity
        if (model()->parameters().elastic_consumption_starting_values) {
            starting_value_quantity[index] =
                starting_value_quantity[index]
                * std::pow(consumption_prices[index] / starting_value_flow.get_price(), input_storage->parameters().consumption_price_elasticity);
        }
        // scale starting value with scaling_value to use in optimization
        if (to_float(baseline_consumption[index].get_quantity()) != 0.0) {
            scaled_starting_value[index] = scale_quantity_to_double(starting_value_quantity[index], baseline_consumption[index].get_quantity());
        } else {
            scaled_starting_value[index] = 0.0;
        }
        // calculate maximum affordable consumption based on budget and possible consumption quantity:
        if (to_float(scaled_starting_value[index]) <= 0.0) {
            lower_bounds[index] = 0.0;
            upper_bounds[index] = 0.0;
        } else {
            FloatType affordable_quantity = to_float(consumption_budget / possible_consumption_quantity * to_float(consumption_prices[index]))
                                            * scale_quantity_to_double(possible_consumption_quantity, baseline_consumption[index].get_quantity());
            lower_bounds[index] = std::min(0.5, scaled_starting_value[index]);
            upper_bounds[index] = std::min(1.5, affordable_quantity);  // constrain consumption optimization between 50% reduction and 50% increase
        }
    }
    // utility optimization
    // set parameters
    optimizer_consumption = std::vector<double>(input_storages.size());  // use normalized variable for optimization to improve?! performance
    // scale xtol_abs
    for (auto& input_storage : input_storages) {
        int index = input_storage->id.index();
        xtol_abs[index] = scale_double_to_double(xtol_abs[index], baseline_consumption[index].get_quantity());
        xtol_abs_global[index] = scale_double_to_double(xtol_abs_global[index], baseline_consumption[index].get_quantity());
        optimizer_consumption[index] = std::min(scaled_starting_value[index], upper_bounds[index]);
    }
    if constexpr (VERBOSE_CONSUMER) {
        log::info(this, "upper bounds");
        for (auto& input_storage : input_storages) {
            log::info(input_storage->name(), "upper bound: ", upper_bounds[input_storage->id.index()]);
        }
    }

    // define local optimizer
    optimization::Optimization local_optimizer(static_cast<nlopt_algorithm>(model()->parameters().utility_optimization_algorithm),
                                               input_storages.size());  // TODO keep and only recreate when resize is needed
    local_optimizer.xtol(xtol_abs);
    local_optimizer.maxeval(model()->parameters().utility_optimization_maxiter);
    local_optimizer.maxtime(model()->parameters().utility_optimization_timeout);
    if (model()->parameters().budget_inequality_constrained) {
        local_optimizer.add_inequality_constraint(this, FlowValue::precision);
    } else {
        local_optimizer.add_equality_constraint(this, FlowValue::precision);
    }
    local_optimizer.add_max_objective(this);
    local_optimizer.lower_bounds(lower_bounds);
    local_optimizer.upper_bounds(upper_bounds);

    if (model()->parameters().global_utility_optimization) {
        // define  lagrangian optimizer to pass (in)equality constraints to global algorithm which cannot use it directly:
        optimization::Optimization lagrangian_optimizer(static_cast<nlopt_algorithm>(model()->parameters().lagrangian_algorithm),
                                                        input_storages.size());  // TODO keep and only recreate when resize is needed
        if (model()->parameters().budget_inequality_constrained) {
            lagrangian_optimizer.add_inequality_constraint(this, FlowValue::precision / to_float(consumption_budget));
        } else {
            lagrangian_optimizer.add_equality_constraint(this, FlowValue::precision / baseline_utility);
        }
        lagrangian_optimizer.add_max_objective(this);
        lagrangian_optimizer.lower_bounds(lower_bounds);
        lagrangian_optimizer.upper_bounds(upper_bounds);
        lagrangian_optimizer.xtol(xtol_abs_global);
        lagrangian_optimizer.maxeval(model()->parameters().global_optimization_maxiter);
        lagrangian_optimizer.maxtime(model()->parameters().optimization_timeout);

        // define global optimizer to use random sampling MLSL algorithm as global search, before local optimization via local_optimizer:
        optimization::Optimization global_optimizer(static_cast<nlopt_algorithm>(model()->parameters().global_utility_optimization_algorithm),
                                                    input_storages.size());  // TODO keep and only recreate when resize is needed

        global_optimizer.xtol(xtol_abs_global);
        global_optimizer.maxeval(model()->parameters().global_utility_optimization_maxiter);
        global_optimizer.maxtime(model()->parameters().global_utility_optimization_timeout);
        global_optimizer.lower_bounds(lower_bounds);
        global_optimizer.upper_bounds(upper_bounds);
        global_optimizer.set_local_algorithm(local_optimizer.get_optimizer());
        global_optimizer.add_max_objective(this);
        nlopt_set_population(
            global_optimizer.get_optimizer(),
            model()->parameters().global_utility_optimization_random_points);  // one might adjust number of random sampling points per iteration
                                                                               // TODO: maybe number of random sampling points should scale with
                                                                               // dimension of the problem
        // start combined global local optimizer optimizer
        lagrangian_optimizer.set_local_algorithm(global_optimizer.get_optimizer());
        consumption_optimize(lagrangian_optimizer);
        optimized_utility = lagrangian_optimizer.optimized_value();
    } else {
        consumption_optimize(local_optimizer);
        optimized_utility = local_optimizer.optimized_value();
    }
    std::vector<Flow> consumption;
    consumption.reserve(input_storages.size());
    for (auto& input_storage : input_storages) {
        int index = input_storage->id.index();
        consumption.emplace_back(invert_scaling_double_to_quantity(optimizer_consumption[index], baseline_consumption[index].get_quantity()),
                                 consumption_prices[index]);
    }
    return std::pair<std::vector<Flow>, FloatType>(consumption, optimized_utility);
}

/**
 * help method to conduct optimization with changing NLOpt optimizers
 * @param optimizer NLOpt based optimizer type
 */
void Consumer::consumption_optimize(optimization::Optimization& optimizer) {
    try {
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, " distribution before optimization");
            for (auto& input_storage : input_storages) {
                log::info(input_storage->name(), " target consumption: ", optimizer_consumption[input_storage->id.index()]);
                log::info(input_storage->name(), " rounded target consumption: ",
                          invert_scaling_double_to_double(optimizer_consumption[input_storage->id.index()],
                                                          baseline_consumption[input_storage->id.index()].get_quantity()));
            }
        }
        const auto res = optimizer.optimize(optimizer_consumption);
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, " distribution after optimization");
            for (auto& input_storage : input_storages) {
                log::info(input_storage->name(), " optimized consumption: ", optimizer_consumption[input_storage->id.index()]);
                log::info(input_storage->name(), " rounded optimized consumption: ",
                          invert_scaling_double_to_double(optimizer_consumption[input_storage->id.index()],
                                                          baseline_consumption[input_storage->id.index()].get_quantity()));
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
                        throw log::error(this, "optimization is roundoff limited (for ", input_storages.size(), " consumption goods)");
                    } else {
                        log::warning(this, "optimization is roundoff limited (for ", input_storages.size(), " consumption goods)");
                    }
                }
            } else if (optimizer.maxeval_reached()) {
                if constexpr (options::DEBUGGING) {
                    debug_print_distribution();
                }
                model()->run()->event(EventType::OPTIMIZER_MAXITER, this);
                if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                    throw log::error(this, "optimization reached maximum iterations (for ", input_storages.size(), " consumption goods)");
                } else {
                    log::warning(this, "optimization reached maximum iterations (for ", input_storages.size(), " consumption goods)");
                }
            } else if (optimizer.maxtime_reached()) {
                if constexpr (options::DEBUGGING) {
                    debug_print_distribution();
                }
                model()->run()->event(EventType::OPTIMIZER_TIMEOUT, this);
                if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                    throw log::error(this, "optimization timed out (for ", input_storages.size(), " consumption goods)");
                } else {
                    log::warning(this, "optimization timed out (for ", input_storages.size(), " consumption goods)");
                }
            } else {
                log::warning(this, "optimization finished with ", optimizer.last_result_description());
            }
        }
    } catch (const optimization::failure& ex) {
        if constexpr (options::DEBUGGING) {
            debug_print_distribution();
        }
        throw log::error(this, "optimization failed, ", ex.what(), " (for ", input_storages.size(), " consumption goods)");
    }
}

/**
 * method to handle consumption steps after optimisation, i.e. storage depletion etc.
 * @param consumption vector of consumption to be handled
 */
void Consumer::consume_optimisation_result(std::vector<Flow> consumption) {
    not_spent_budget += consumption_budget;
    for (auto& input_storage : input_storages) {
        int r = input_storage->id.index();
        input_storage->set_desired_used_flow_U_tilde(consumption[r]);
        input_storage->use_content_S(consumption[r]);
        region->add_consumption_flow_Y(consumption[r]);
        input_storage->iterate_consumption_and_production();
        previous_consumption[r] = consumption[r];
        not_spent_budget -= consumption[r].get_value();
    }
    not_spent_budget = FlowValue(0.0);
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
            std::vector<double> grad_constraint(input_storages.size());
            std::vector<double> grad_objective(input_storages.size());

            std::cout << model()->run()->timeinfo() << ", " << name() << ": demand distribution for " << input_storages.size() << " inputs :\n";
            print_row("inequality value", inequality_constraint(&optimizer_consumption[0], &grad_constraint[0]));
            print_row("objective value", max_objective(&optimizer_consumption[0], &grad_objective[0]));
            print_row("current utility", utility);
            print_row("current total budget", consumption_budget);
            print_row("substitution coefficient", inter_basket_substitution_coefficient);
            std::cout << '\n';

            for (auto& input_storage : input_storages) {
                int r = input_storage->id.index();
                std::cout << "    " << input_storage->name() << " :\n";
                print_row("grad (constraint)", grad_constraint[r]);
                print_row("grad (objective)", grad_objective[r]);
                print_row("share factor", share_factors[r]);
                print_row("consumption quantity", invert_scaling_double_to_quantity(optimizer_consumption[r], baseline_consumption[r].get_quantity()));
                print_row("share of start cons. qty(%)", optimizer_consumption[r]);
                print_row("consumption price", consumption_prices[r]);
                print_row("consumption value",
                          invert_scaling_double_to_quantity(optimizer_consumption[r], baseline_consumption[r].get_quantity()) * consumption_prices[r]);
                std::cout << '\n';
            }
        }
    }
}

FlowQuantity Consumer::invert_scaling_double_to_quantity(double scaled_value, FlowQuantity scaling_quantity) { return scaled_value * scaling_quantity; }
double Consumer::invert_scaling_double_to_double(double scaled_value, FlowQuantity scaling_quantity) { return scaled_value * to_float(scaling_quantity); }
double Consumer::scale_quantity_to_double(FlowQuantity quantity, FlowQuantity scaling_quantity) { return to_float(quantity / scaling_quantity); }
double Consumer::scale_double_to_double(double not_scaled_double, FlowQuantity scaling_quantity) { return not_scaled_double / to_float(scaling_quantity); }

}  // namespace acclimate
