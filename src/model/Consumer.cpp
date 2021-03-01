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

void Consumer::initialize() {
    debug::assertstep(this, IterationStep::INITIALIZATION);

    for (std::size_t r = 0; r < input_storages.size(); ++r) {
        input_storage_int_map.insert(std::pair<hash_t, int>(input_storages[r]->id.name_hash, r));
        input_storage_sector_map.insert(std::pair<Sector*, hash_t>(input_storages[r]->sector, r));
    }

    autodiffutility = {input_storages.size(), 0.0};
    autodiff_basket_consumption_utility = {input_storages.size(), 0.0};
    autodiff_consumption_utility = {input_storages.size(), 0.0};
    var_optimizer_consumption = autodiff::Variable<FloatType>(0, input_storages.size(), input_storages.size(), 0.0);

    share_factors = std::vector<FloatType>(input_storages.size());
    previous_consumption.reserve(input_storages.size());
    baseline_consumption.reserve(input_storages.size());

    // initialize previous consumption as starting values, calculate budget
    consumption_budget = FlowValue(0.0);
    not_spent_budget = FlowValue(0.0);
    for (auto& input_storage : input_storages) {
        if constexpr (VERBOSE_CONSUMER) {
            log::info(input_storage->name());
        }
        const auto initial_consumption = input_storage->initial_used_flow_U_star();
        previous_consumption.push_back(initial_consumption);
        baseline_consumption.push_back(initial_consumption);
    }
    for (auto& input_storage : input_storages) {
        int r = input_storage_int_map.find(input_storage->id.name_hash)->second;
        const auto initial_consumption = input_storage->initial_used_flow_U_star();
        consumption_budget += initial_consumption.get_value();
        previous_consumption[r] = initial_consumption;  // overwrite to get proper ordering with regard to input_storage[r]. workaround since no constructor for
                                                        // std::vector<Flow> available
        baseline_consumption[r] = initial_consumption;
    }

    if constexpr (VERBOSE_CONSUMER) {
        log::info("budget: ", consumption_budget);
    }
    // initialize share factors
    for (auto& input_storage : input_storages) {
        int r = input_storage_int_map.find(input_storage->id.name_hash)->second;
        share_factors[r] = to_float(input_storage->initial_used_flow_U_star().get_value()) / to_float(consumption_budget);
    }
    intra_basket_substitution_coefficient = std::vector<FloatType>(consumer_baskets.size(), 0);
    intra_basket_substitution_exponent = std::vector<FloatType>(consumer_baskets.size(), 0);
    basket_share_factors = std::vector<FloatType>(consumer_baskets.size(), 0);
    for (std::size_t basket = 0; basket < consumer_baskets.size(); ++basket) {
        intra_basket_substitution_coefficient[basket] = (consumer_baskets[basket].second);
        for (auto& sector : consumer_baskets[basket].first) {
            Storage* current_storage = input_storages.find(input_storage_name(sector));
            basket_share_factors[basket] += to_float(current_storage->initial_used_flow_U_star().get_value()) / to_float(consumption_budget);
        }

        basket_share_factors[basket] =
            std::pow(basket_share_factors[basket], 1 / inter_basket_substitution_coefficient);  // already with exponent for utility function
        intra_basket_substitution_exponent[basket] = (intra_basket_substitution_coefficient[basket] - 1) / intra_basket_substitution_coefficient[basket];
    }
    baseline_utility = CES_utility_function(baseline_consumption);  // baseline utility for scaling
}

// TODO: more sophisticated scaling than normalizing with baseline utility might improve optimization?!

// nested utility function: goods are classified e.g. into necessary, relevant and other goods.

autodiff::Value<FloatType> Consumer::autodiff_nested_CES_utility_function(const autodiff::Variable<FloatType>& consumption_demands) {
    autodiff_consumption_utility = {input_storages.size(), 0.0};
    for (std::size_t basket = 0; basket < consumer_baskets.size(); ++basket) {
        autodiff_basket_consumption_utility = {input_storages.size(), 0.0};
        for (auto& sector : consumer_baskets[basket].first) {
            Storage* current_storage = input_storages.find(input_storage_name(sector));
            int r = input_storage_int_map.find(current_storage->id.name_hash)->second;
            autodiff_basket_consumption_utility += std::pow(consumption_demands[r], intra_basket_substitution_exponent[basket]) * share_factors[r];
        }
        autodiff_basket_consumption_utility = std::pow(autodiff_basket_consumption_utility, 1 / intra_basket_substitution_exponent[basket]);
        autodiff_consumption_utility += std::pow(autodiff_basket_consumption_utility, inter_basket_substitution_exponent) * basket_share_factors[basket];
    }
    return autodiff_consumption_utility;  // outer exponent not relevant for optimization (at least for sigma >1)
}

FloatType Consumer::CES_utility_function(const std::vector<FloatType>& consumption_demands) {
    consumption_utility = 0.0;
    for (std::size_t basket = 0; basket < consumer_baskets.size(); ++basket) {
        basket_consumption_utility = 0.0;
        for (auto& sector : consumer_baskets[basket].first) {
            Storage* current_storage = input_storages.find(input_storage_name(sector));
            int r = input_storage_int_map.find(current_storage->id.name_hash)->second;
            basket_consumption_utility += std::pow(consumption_demands[r], intra_basket_substitution_exponent[basket]) * share_factors[r];
        }
        basket_consumption_utility = std::pow(basket_consumption_utility, 1 / intra_basket_substitution_exponent[basket]);
        consumption_utility += std::pow(basket_consumption_utility, inter_basket_substitution_exponent) * basket_share_factors[basket];
    }
    return consumption_utility;  // outer exponent not relevant for optimization (at least for sigma >1)
}
FloatType Consumer::CES_utility_function(const std::vector<Flow>& consumption_demands) {
    consumption_utility = 0.0;
    for (std::size_t basket = 0; basket < consumer_baskets.size(); ++basket) {
        basket_consumption_utility = 0.0;
        for (auto& sector : consumer_baskets[basket].first) {
            Storage* current_storage = input_storages.find(input_storage_name(sector));
            int r = input_storage_int_map.find(current_storage->id.name_hash)->second;
            basket_consumption_utility +=
                std::pow(to_float(consumption_demands[r].get_quantity()), intra_basket_substitution_exponent[basket]) * share_factors[r];
        }
        basket_consumption_utility = std::pow(basket_consumption_utility, 1 / intra_basket_substitution_exponent[basket]);
        consumption_utility += std::pow(basket_consumption_utility, inter_basket_substitution_exponent) * basket_share_factors[basket];
    }
    return consumption_utility;  // outer exponent not relevant for optimization (at least for sigma >1)
}

// TODO: more complex budget function might be useful, e.g. opportunity for saving etc.
FloatType Consumer::equality_constraint(const double* x, double* grad) { return inequality_constraint(x, grad); }

FloatType Consumer::inequality_constraint(const double* x, double* grad) {
    double scaled_budget = (consumption_budget + not_spent_budget) / consumption_budget;
    for (auto& input_storage : input_storages) {
        int r = input_storage_int_map.find(input_storage->id.name_hash)->second;
        assert(!std::isnan(x[r]));
        scaled_budget -= to_float(invert_scaling_double_to_quantity(x[r], baseline_consumption[r].get_quantity())) * to_float(consumption_prices[r])
                         / to_float(consumption_budget);
        if (grad != nullptr) {
            grad[r] = to_float(invert_scaling_double_to_quantity(x[r], baseline_consumption[r].get_quantity())) * to_float(consumption_prices[r])
                      / to_float(consumption_budget);
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
    for (auto& input_storage : input_storages) {
        int r = input_storage_int_map.find(input_storage->id.name_hash)->second;
        assert(!std::isnan(x[r]));
        var_optimizer_consumption.value()[r] = invert_scaling_double_to_quantity(x[r], baseline_consumption[r].get_quantity());
    }
    autodiffutility = autodiff_nested_CES_utility_function(var_optimizer_consumption);
    if (grad != nullptr) {
        std::copy(std::begin(autodiffutility.derivative()), std::end(autodiffutility.derivative()), grad);
        for (auto& input_storage : input_storages) {
            int r = input_storage_int_map.find(input_storage->id.name_hash)->second;
            grad[r] = grad[r] / baseline_utility;
        }
        if constexpr (options::OPTIMIZATION_WARNINGS) {
            for (auto& input_storage : input_storages) {
                int r = input_storage_int_map.find(input_storage->id.name_hash)->second;
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
    std::vector<Flow> consumption;
    consumption.reserve(input_storages.size());
    if (utilitarian) {
        consumption = utilitarian_consumption_optimization();
        utilitarian_consumption_execution(consumption);
        local_optimal_utility = CES_utility_function(consumption) / baseline_utility;  // just making sure the field has data, identical to utility in this case
    } else {
        // calculate local optimal utility consumption for comparison:
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, "local utilitarian consumption optimization:");
        }
        local_optimal_utility = CES_utility_function(utilitarian_consumption_optimization()) / baseline_utility;
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
    utility = CES_utility_function(consumption) / baseline_utility;
    log::info(this, "baseline relative utility:", utility);
}

std::vector<Flow> Consumer::utilitarian_consumption_optimization() {
    std::vector<Price> current_prices;
    std::vector<FlowQuantity> starting_value_quantity(input_storages.size());
    std::vector<FloatType> scaled_starting_value = std::vector<FloatType>(input_storages.size());

    // optimization parameters
    std::vector<FloatType> xtol_abs(input_storages.size(), FlowQuantity::precision);
    std::vector<FloatType> xtol_abs_global(input_storages.size(), FlowQuantity::precision * 100);
    std::vector<FloatType> lower_bounds(input_storages.size(),
                                        0.00001);  // lower bound of exactly 0 is violated by Nlopt - bug?! - thus keeping minimum consumption level
    std::vector<FloatType> upper_bounds = std::vector<FloatType>(input_storages.size());

    // global fields to improve performance of optimization
    consumption_prices.clear();
    consumption_prices = std::vector<Price>(input_storages.size());

    for (auto& input_storage : input_storages) {
        int r = input_storage_int_map.find(input_storage->id.name_hash)->second;
        FlowQuantity possible_consumption_quantity = (input_storage->get_possible_use_U_hat().get_quantity());
        consumption_prices[r] = input_storage->get_possible_use_U_hat().get_price();
        // TODO: extend by alternative prices of most recent goods, e.g.
        // consumption_prices.push_back(input_storage->current_input_flow_I().get_price());// using this would be giving the consumer some "foresight"

        /*
        start optimization at previous consumption, guarantees stability in undisturbed baseline
        while potentially speeding up optimization in case of small price changes*/
        auto starting_value_flow = (to_float(previous_consumption[r].get_quantity()) == 0.0) ? baseline_consumption[r] : previous_consumption[r];
        starting_value_quantity[r] = starting_value_flow.get_quantity();
        starting_value_quantity[r] = std::min(starting_value_quantity[r], possible_consumption_quantity);
        // adjust if price changes make previous consumption to expensive - scaling with elasticity
        // TODO: one could try a more sophisticated use of price elasticity
        starting_value_quantity[r] =
            starting_value_quantity[r]
            * std::pow(to_float(consumption_prices[r]) / to_float(starting_value_flow.get_price()), input_storage->parameters().consumption_price_elasticity);
        // calculate maximum affordable consumption based on budget and possible consumption quantity:
        if (to_float(starting_value_quantity[r]) <= 0.0) {
            upper_bounds[r] = 0;
        } else {
            upper_bounds[r] = std::min(1.0, to_float(consumption_budget) / (to_float(possible_consumption_quantity) * to_float(consumption_prices[r])))
                              * (to_float(possible_consumption_quantity) / to_float(baseline_consumption[r].get_quantity()));
        }
        // scale starting value with scaling_value to use in optimization
        scaled_starting_value[r] = scale_quantity_to_double(starting_value_quantity[r], baseline_consumption[r].get_quantity());
    }
    // utility optimization
    // set parameters

    optimizer_consumption = std::vector<double>(input_storages.size());  // use normalized variable for optimization to improve?! performance
    // scale xtol_abs
    for (auto& input_storage : input_storages) {
        int r = input_storage_int_map.find(input_storage->id.name_hash)->second;
        xtol_abs[r] = scale_double_to_double(xtol_abs[r], baseline_consumption[r].get_quantity());
        xtol_abs_global[r] = scale_double_to_double(xtol_abs_global[r], baseline_consumption[r].get_quantity());
        optimizer_consumption[r] = std::min(scaled_starting_value[r], upper_bounds[r]);
    }
    if constexpr (VERBOSE_CONSUMER) {
        log::info(this, "upper bounds");
        for (auto& input_storage : input_storages) {
            int r = int(input_storage_int_map.find(input_storage->id.name_hash)->second);
            log::info(input_storage->name(), "upper bound: ", upper_bounds[r]);
        }
    }

    // define local optimizer
    optimization::Optimization local_optimizer(static_cast<nlopt_algorithm>(model()->parameters().utility_optimization_algorithm),
                                               input_storages.size());  // TODO keep and only recreate when resize is needed
    // set parameters
    local_optimizer.xtol(xtol_abs);
    local_optimizer.maxeval(model()->parameters().optimization_maxiter);
    local_optimizer.maxtime(model()->parameters().optimization_timeout);

    if (model()->parameters().global_optimization) {
        // define  lagrangian optimizer to pass (in)equality constraints to global algorithm which cant use it directly:
        optimization::Optimization lagrangian_optimizer(static_cast<nlopt_algorithm>(model()->parameters().lagrangian_algorithm),
                                                        input_storages.size());  // TODO keep and only recreate when resize is needed
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
                                                    input_storages.size());  // TODO keep and only recreate when resize is needed

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
    std::vector<Flow> consumption;
    consumption.reserve(input_storages.size());
    for (auto& input_storage : input_storages) {
        int r = input_storage_int_map.find(input_storage->id.name_hash)->second;
        consumption.emplace_back(FlowQuantity(invert_scaling_double_to_quantity(optimizer_consumption[r], baseline_consumption[r].get_quantity())),
                                 consumption_prices[r]);
    }
    return consumption;
}

void Consumer::consumption_optimize(optimization::Optimization& optimizer) {
    try {
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, " distribution before optimization");
            for (auto& input_storage : input_storages) {
                int r = input_storage_int_map.find(input_storage->id.name_hash)->second;
                log::info(input_storage->name(), " target consumption: ", optimizer_consumption[r]);
            }
        }
        const auto res = optimizer.optimize(optimizer_consumption);
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, " distribution after optimization");
            for (auto& input_storage : input_storages) {
                int r = input_storage_int_map.find(input_storage->id.name_hash)->second;
                log::info(input_storage->name(), " optimized consumption: ", optimizer_consumption[r]);
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

void Consumer::utilitarian_consumption_execution(std::vector<Flow> consumption_flows) {
    // initialize available budget
    not_spent_budget += consumption_budget;
    for (auto& input_storage : input_storages) {
        int r = input_storage_int_map.find(input_storage->id.name_hash)->second;
        // withdraw consumption from storage
        input_storage->set_desired_used_flow_U_tilde((consumption_flows[r]));
        input_storage->use_content_S((consumption_flows[r]));
        region->add_consumption_flow_Y((consumption_flows[r]));
        // consume goods
        input_storage->iterate_consumption_and_production();
        // adjust previous consumption
        previous_consumption[r] = (consumption_flows[r]);
        // adjust non-spent budget
        not_spent_budget -= consumption_flows[r].get_value();
    }
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
            print_row("current utility", utility / baseline_utility);
            print_row("current total budget", consumption_budget);
            print_row("substitution coefficient", inter_basket_substitution_coefficient);
            std::cout << '\n';

            for (auto& input_storage : input_storages) {
                int r = input_storage_int_map.find(input_storage->id.name_hash)->second;
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

double Consumer::invert_scaling_double_to_quantity(double scaled_value, FlowQuantity scaling_quantity) const {
    return to_float(scaled_value * scaling_quantity);
}
double Consumer::scale_quantity_to_double(FlowQuantity quantity, FlowQuantity scaling_quantity) const { return to_float(quantity / scaling_quantity); }
double Consumer::scale_double_to_double(double not_scaled_double, FlowQuantity scaling_quantity) const {
    return not_scaled_double / to_float(scaling_quantity);
}

std::string Consumer::input_storage_name(Sector* sector) {
    return input_storages[input_storage_int_map.find(input_storage_sector_map.find(sector)->second)->second]->name();
}

}  // namespace acclimate
