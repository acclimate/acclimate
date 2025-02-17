// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/Consumer.h"

#include <cmath>

#include "ModelRun.h"
#include "acclimate.h"
#include "autodiff.h"
#include "model/Model.h"
#include "model/Region.h"
#include "model/Storage.h"
#include "optimization.h"

static constexpr auto MAX_GRADIENT = 1e3;  // TODO: any need to modify?
static constexpr bool IGNORE_ROUNDOFFLIMITED = false;
static constexpr bool VERBOSE_CONSUMER = true;

namespace acclimate {
// TODO: in the long run abstract consumer with different implementations would be nice

Consumer::Consumer(id_t id,
                   Region* region,
                   FloatType inter_basket_substitution_coefficient,
                   std::vector<std::pair<std::vector<Sector*>, FloatType>> consumer_baskets,
                   bool utilitarian)
    : EconomicAgent(std::move(id), region, EconomicAgent::type_t::CONSUMER),
      utilitarian_(utilitarian),
      consumer_baskets_(std::move(consumer_baskets)),
      inter_basket_substitution_coefficient_(inter_basket_substitution_coefficient),
      inter_basket_substitution_exponent_((inter_basket_substitution_coefficient_ - 1) / inter_basket_substitution_coefficient_) {}

/**
 * initializes parameters of utility_ function depending on current input storages and
 * fields used in optimization methods to improve performance
 */
void Consumer::initialize() {
    debug::assertstep(this, IterationStep::INITIALIZATION);

    autodiffutility_ = {input_storages.size(), 0.0};
    autodiff_basket_consumption_utility_ = {input_storages.size(), 0.0};
    autodiff_consumption_utility_ = {input_storages.size(), 0.0};

    var_optimizer_consumption_ = autodiff::Variable<FloatType>(input_storages.size(), 0.0);

    share_factors_ = std::vector<FloatType>(input_storages.size());
    exponent_share_factors_ = std::vector<FloatType>(input_storages.size());
    previous_consumption_.reserve(input_storages.size());
    baseline_consumption_.reserve(input_storages.size());

    consumption_budget_ = FlowValue(0.0);
    not_spent_budget_ = FlowValue(0.0);

    for (auto& input_storage : input_storages) {
        previous_consumption_.push_back(input_storage->baseline_used_flow());
        baseline_consumption_.push_back(input_storage->baseline_used_flow());
        consumption_budget_ += input_storage->baseline_used_flow().get_value();
    }

    if constexpr (VERBOSE_CONSUMER) {
        log::info("budget: ", consumption_budget_);
    }

    intra_basket_substitution_coefficient_ = std::vector<FloatType>(consumer_baskets_.size(), 0);
    intra_basket_substitution_exponent_ = std::vector<FloatType>(consumer_baskets_.size(), 0);
    basket_share_factors_ = std::vector<FloatType>(consumer_baskets_.size(), 0);
    exponent_basket_share_factors_ = std::vector<FloatType>(consumer_baskets_.size(), 0);
    consumer_basket_indizes_ = std::vector<std::vector<int>>(consumer_baskets_.size());
    for (int basket = 0; basket < static_cast<int>(consumer_baskets_.size()); ++basket) {
        intra_basket_substitution_coefficient_[basket] = (consumer_baskets_[basket].second);
        intra_basket_substitution_exponent_[basket] = (intra_basket_substitution_coefficient_[basket] - 1) / intra_basket_substitution_coefficient_[basket];
        for (auto& sector : consumer_baskets_[basket].first) {
            for (auto& i_storage : input_storages) {
                if (i_storage->sector == sector) {
                    basket_share_factors_[basket] += to_float(i_storage->baseline_used_flow().get_value()) / to_float(consumption_budget_);
                }
            }
        }
    }
    // edge case of no consumption in this basket - remove basket from optimization
    auto original_basket_number = consumer_baskets_.size();
    log::info(this, "basketnumber: ", original_basket_number);
    for (int basket = original_basket_number; basket >= 0; basket--) {
        if (basket_share_factors_[basket] == 0) {
            log::info(this, "basket: ", basket, " removed");
            consumer_baskets_.erase(std::remove(consumer_baskets_.begin(), consumer_baskets_.end(), consumer_baskets_[basket]), consumer_baskets_.end());
        }
    }
    auto consumption_in_relevant_baskets = FlowValue(0.0);
    for (auto& consumer_basket : consumer_baskets_) {
        for (auto& sector : consumer_basket.first) {
            for (auto& i_storage : input_storages) {
                if (i_storage->sector == sector) {
                    consumption_in_relevant_baskets += i_storage->baseline_used_flow().get_value();
                }
            }
        }
    }

    std::vector<Sector*> all_relevant_sectors;
    for (int basket = 0; basket < static_cast<int>(consumer_baskets_.size()); ++basket) {
        for (auto& sector : consumer_baskets_[basket].first) {
            for (auto& i_storage : input_storages) {
                if (i_storage->sector == sector) {
                    basket_share_factors_[basket] = basket_share_factors_[basket] * to_float(consumption_budget_ / consumption_in_relevant_baskets);
                    all_relevant_sectors.push_back(sector);
                }
            }
        }
    }

    for (auto& input_storage : input_storages) {
        if (std::count(all_relevant_sectors.begin(), all_relevant_sectors.end(), input_storage->sector) != 0) {
            share_factors_[input_storage->id.index()] = to_float(input_storage->baseline_used_flow().get_value() / consumption_budget_)
                                                        * to_float(consumption_budget_ / consumption_in_relevant_baskets);
            previous_consumption_[input_storage->id.index()] =
                input_storage->baseline_used_flow();  // overwrite to get proper ordering with regard to input_storage[index].
            // workaround since no constructor for std::vector<Flow> available
            baseline_consumption_[input_storage->id.index()] = input_storage->baseline_used_flow();
        } else {
            share_factors_[input_storage->id.index()] = 0;
            previous_consumption_[input_storage->id.index()] = Flow(0);
            baseline_consumption_[input_storage->id.index()] = Flow(0);
            log::info(this, "sector: ", input_storage->sector->id, " set to 0");
        }
    }

    for (int basket = 0; basket < static_cast<int>(consumer_baskets_.size()); ++basket) {
        for (auto& sector : consumer_baskets_[basket].first) {
            for (auto& i_storage : input_storages) {
                if (i_storage->sector == sector) {
                    consumer_basket_indizes_[basket].push_back(i_storage->id.index());
                    share_factors_[i_storage->id.index()] =
                        share_factors_[i_storage->id.index()] / basket_share_factors_[basket];  // normalize share factors of a basket to 1

                    exponent_share_factors_[i_storage->id.index()] =
                        std::pow(share_factors_[i_storage->id.index()],
                                 1 / intra_basket_substitution_coefficient_[basket]);  // already with exponent for utility_ function
                }
            }
        }

        exponent_basket_share_factors_[basket] =
            std::pow(basket_share_factors_[basket], 1 / inter_basket_substitution_coefficient_);  // already with exponent for utility_ function
    }
}

// TODO: more sophisticated scaling than normalizing with baseline utility_ might improve optimization?!
/**
 * nested CES utility_ function using automatic differentiation to enable fast gradient calculation
 * @param consumption vector of consumed quantity of each good
 * @return auto-differentiable value of the utility_ function
 */
auto Consumer::autodiff_nested_CES_utility_function(const autodiff::Variable<FloatType>& baseline_relative_consumption) -> autodiff::Value<FloatType> {
    autodiff_consumption_utility_ = {input_storages.size(), 0.0};  // reset to 0 without new call
    for (int basket = 0; basket < static_cast<int>(consumer_baskets_.size()); ++basket) {
        autodiff_basket_consumption_utility_ = {input_storages.size(), 0.0};
        for (auto& index : consumer_basket_indizes_[basket]) {
            auto consumption_quantity = baseline_relative_consumption[index] * share_factors_[index];
            autodiff_basket_consumption_utility_ +=
                std::pow(consumption_quantity, intra_basket_substitution_exponent_[basket]) * exponent_share_factors_[index];
        }
        autodiff_basket_consumption_utility_ =
            std::pow(autodiff_basket_consumption_utility_, 1 / intra_basket_substitution_exponent_[basket]) * basket_share_factors_[basket];

        autodiff_consumption_utility_ +=
            std::pow(autodiff_basket_consumption_utility_, inter_basket_substitution_exponent_) * exponent_basket_share_factors_[basket];
    }
    autodiff_consumption_utility_ = std::pow(autodiff_consumption_utility_, 1 / inter_basket_substitution_exponent_);

    return autodiff_consumption_utility_;
}

// TODO: more complex budget function might be useful, e.g. opportunity for saving etc.
/**
 * if budget should be spent always, equality constrained can be used instead of inequality constraint
 * @param x NLOpt convention of a c-style vector of consumption
 * @param grad gradient vector
 * @return value of the constraint, which is targeted to be =0 by NLOpt optimizers
 */
auto Consumer::equality_constraint(const double* x, double* grad) -> FloatType { return inequality_constraint(x, grad); }
/**
 * inequality constraint should be sufficient for utility_ optimization, since more spending -> more consumption should be induced by the objective anyways
 * @param x NLOpt convention of a c-style vector of consumption
 * @param grad gradient vector
 * @return value of the constraint, which is targeted to be <=0 by NLOpt optimizers
 */
auto Consumer::inequality_constraint(const double* x, double* grad) -> FloatType {
    double scaled_budget = (consumption_budget_ + not_spent_budget_) / consumption_budget_;
    for (auto& input_storage : input_storages) {
        int const index = input_storage->id.index();
        assert(!std::isnan(x[index]));
        double price = NAN;
        if (model()->parameters().elastic_budget) {
            price = x[index] * std::pow(to_float(consumption_prices_[index]), -1 * input_storage->parameters().consumption_price_elasticity);
        } else {
            price = x[index] * to_float(consumption_prices_[index]);
        }
        scaled_budget -= invert_scaling_double_to_double(x[index], baseline_consumption_[index].get_quantity()) * price / to_float(consumption_budget_);
        if (grad != nullptr) {
            grad[index] = invert_scaling_double_to_double(x[index], baseline_consumption_[index].get_quantity()) * price / to_float(consumption_budget_);
            if constexpr (Options::OPTIMIZATION_WARNINGS) {
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
 * objective function for utility_ maximisation, using automatic-differentiable utility_ function
 * @param x NLOpt convention of a c-style vector of consumption
 * @param grad gradient vector
 * @return value of the objective function, which is maximized
 */
auto Consumer::max_objective(const double* x, double* grad) -> FloatType {
    for (auto& input_storage : input_storages) {
        int const index = input_storage->id.index();
        assert(!std::isnan(x[index]));
        var_optimizer_consumption_.value()[index] = x[index];
    }
    autodiffutility_ = autodiff_nested_CES_utility_function(var_optimizer_consumption_);
    if (grad != nullptr) {
        std::copy(std::begin(autodiffutility_.derivative()), std::end(autodiffutility_.derivative()), grad);
        if constexpr (Options::OPTIMIZATION_WARNINGS) {
            for (auto& input_storage : input_storages) {
                int const index = input_storage->id.index();
                assert(!std::isnan(grad[index]));
                if (grad[index] > MAX_GRADIENT) {
                    log::warning(this, ": large gradient of ", grad[index]);
                }
            }
        }
    }
    return autodiffutility_.value();
}

void Consumer::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    std::vector<Flow> consumption;
    consumption.reserve(input_storages.size());
    if (utilitarian_) {
        auto optimization_result = utilitarian_consumption_optimization();
        consumption = optimization_result.first;

        consume_optimisation_result(consumption);
        local_optimal_utility_ = optimization_result.second;  // just making sure the field has data, identical to utility_ in this case
    } else {
        // calculate local optimal utility_ consumption for comparison:
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, "local utilitarian_ consumption optimization:");
        }
        local_optimal_utility_ = utilitarian_consumption_optimization().second;
        // old consumer to be used for comparison
        previous_consumption_.clear();
        previous_consumption_.reserve(input_storages.size());
        consumption.clear();
        consumption.reserve(input_storages.size());
        for (const auto& is : input_storages) {
            Flow const possible_used_flow = is->get_possible_use();  // Price(U_hat) = Price of used flow
            Price reservation_price;
            if (possible_used_flow.get_quantity() > 0.0) {
                // we have to purchase with the average price of input and storage
                reservation_price = possible_used_flow.get_price();
            } else {  // possible used flow is zero
                Price const last_reservation_price = is->desired_used_flow(this).get_price();
                assert(!isnan(last_reservation_price));
                // price is calculated from last desired used flow
                reservation_price = last_reservation_price;
                model()->run()->event(EventType::NO_CONSUMPTION, this, nullptr, to_float(last_reservation_price));
            }
            assert(reservation_price > 0.0);

            const Flow desired_used_flow = Flow(round(is->baseline_input_flow().get_quantity() * forcing_
                                                      * std::pow(reservation_price / Price(1.0), is->parameters().consumption_price_elasticity)),
                                                reservation_price);
            const Flow used_flow = Flow(FlowQuantity(std::min(desired_used_flow.get_quantity(), possible_used_flow.get_quantity())), reservation_price);
            is->set_desired_used_flow(desired_used_flow);
            is->use_content(round(used_flow));
            region->add_consumption(round(used_flow));
            is->iterate_consumption_and_production();
            consumption.push_back((used_flow));
            previous_consumption_.emplace_back((used_flow.get_quantity()));
        }
    }
    utility_ = local_optimal_utility_;  // TODO: determine whether this separation should be maintained
    log::info(this, "utility_:", utility_);
}

/**
 * NLOpt based optimization of consumption with respect to utility_ function
 * @return vector of utility_ maximising consumption flows
 */
auto Consumer::utilitarian_consumption_optimization() -> std::pair<std::vector<Flow>, FloatType> {
    std::vector<Price> const current_prices;
    std::vector<FlowQuantity> starting_value_quantity(input_storages.size());
    std::vector<FloatType> scaled_starting_value = std::vector<FloatType>(input_storages.size());

    // optimization parameters
    std::vector<FloatType> xtol_abs(input_storages.size(), FlowQuantity::precision * model()->parameters().utility_optimization_precision_adjustment);
    std::vector<FloatType> xtol_abs_global(input_storages.size(),
                                           FlowQuantity::precision * model()->parameters().global_utility_optimization_precision_adjustment);
    std::vector<FloatType> lower_bounds(input_storages.size());
    std::vector<FloatType> upper_bounds = std::vector<FloatType>(input_storages.size());

    FloatType optimized_utility = NAN;

    // global fields to improve performance of optimization
    consumption_prices_.clear();
    consumption_prices_ = std::vector<Price>(input_storages.size());

    for (auto& input_storage : input_storages) {
        int const index = input_storage->id.index();
        FlowQuantity const possible_consumption_quantity = (input_storage->get_possible_use().get_quantity());
        consumption_prices_[index] = input_storage->get_possible_use().get_price();
        // TODO: extend by alternative prices of most recent goods, e.g.
        // consumption_prices_.push_back(input_storage->current_input_flow_I().get_price());// using this would be giving the consumer some "foresight"
        /*
        start optimization at previous consumption, guarantees stability in undisturbed baseline
        while potentially speeding up optimization in case of small price changes*/
        Flow const starting_value_flow =
            (to_float(previous_consumption_[index].get_quantity()) == 0.0) ? baseline_consumption_[index] : previous_consumption_[index];
        starting_value_quantity[index] = std::min(starting_value_flow.get_quantity(), possible_consumption_quantity);
        // adjust if price changes make previous consumption to expensive - scaling with elasticity
        // TODO: one could try a more sophisticated use of price elasticity
        starting_value_quantity[index] =
            starting_value_quantity[index]
            * std::pow(consumption_prices_[index] / starting_value_flow.get_price(), input_storage->parameters().consumption_price_elasticity);
        // scale starting value with scaling_value to use in optimization
        if (to_float(baseline_consumption_[index].get_quantity()) != 0.0) {
            scaled_starting_value[index] = scale_quantity_to_double(starting_value_quantity[index], baseline_consumption_[index].get_quantity());
        } else {
            scaled_starting_value[index] = 0.0;
        }
        // calculate maximum affordable consumption based on budget and possible consumption quantity:
        if (to_float(scaled_starting_value[index]) <= 0.0) {
            lower_bounds[index] = 0.0;
            upper_bounds[index] = 0.0;
        } else {
            FloatType const affordable_quantity = to_float(consumption_budget_ / possible_consumption_quantity * to_float(consumption_prices_[index]))
                                                  * scale_quantity_to_double(possible_consumption_quantity, baseline_consumption_[index].get_quantity());
            lower_bounds[index] = std::min(0.5, scaled_starting_value[index]);
            upper_bounds[index] = std::min(1.5, affordable_quantity);  // constrain consumption optimization between 50% reduction and 50% increase
        }
    }
    // utility_ optimization
    // set parameters
    optimizer_consumption_ = std::vector<double>(input_storages.size());  // use normalized variable for optimization to improve?! performance
    // scale xtol_abs
    for (auto& input_storage : input_storages) {
        int const index = input_storage->id.index();
        xtol_abs[index] = scale_double_to_double(xtol_abs[index], baseline_consumption_[index].get_quantity());
        xtol_abs_global[index] = scale_double_to_double(xtol_abs_global[index], baseline_consumption_[index].get_quantity());
        optimizer_consumption_[index] = std::min(scaled_starting_value[index], upper_bounds[index]);
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
            lagrangian_optimizer.add_inequality_constraint(this, FlowValue::precision / to_float(consumption_budget_));
        } else {
            lagrangian_optimizer.add_equality_constraint(this, FlowValue::precision / baseline_utility_);
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
        int const index = input_storage->id.index();
        consumption.emplace_back(invert_scaling_double_to_quantity(optimizer_consumption_[index], baseline_consumption_[index].get_quantity()),
                                 consumption_prices_[index]);
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
                log::info(input_storage->name(), " target consumption: ", optimizer_consumption_[input_storage->id.index()]);
                log::info(input_storage->name(), " rounded target consumption: ",
                          invert_scaling_double_to_double(optimizer_consumption_[input_storage->id.index()],
                                                          baseline_consumption_[input_storage->id.index()].get_quantity()));
            }
        }
        const auto res = optimizer.optimize(optimizer_consumption_);
        if constexpr (VERBOSE_CONSUMER) {
            log::info(this, " distribution after optimization");
            for (auto& input_storage : input_storages) {
                log::info(input_storage->name(), " optimized consumption: ", optimizer_consumption_[input_storage->id.index()]);
                log::info(input_storage->name(), " rounded optimized consumption: ",
                          invert_scaling_double_to_double(optimizer_consumption_[input_storage->id.index()],
                                                          baseline_consumption_[input_storage->id.index()].get_quantity()));
            }
        }

        if (!res && !optimizer.xtol_reached()) {
            if (optimizer.roundoff_limited()) {
                if constexpr (!IGNORE_ROUNDOFFLIMITED) {
                    if constexpr (Options::DEBUGGING) {
                        debug_print_distribution();
                    }
                    model()->run()->event(EventType::OPTIMIZER_ROUNDOFF_LIMITED, this);
                    if constexpr (Options::OPTIMIZATION_PROBLEMS_FATAL) {
                        throw log::error(this, "optimization is roundoff limited (for ", input_storages.size(), " consumption goods)");
                    } else {
                        log::warning(this, "optimization is roundoff limited (for ", input_storages.size(), " consumption goods)");
                    }
                }
            } else if (optimizer.maxeval_reached()) {
                if constexpr (Options::DEBUGGING) {
                    debug_print_distribution();
                }
                model()->run()->event(EventType::OPTIMIZER_MAXITER, this);
                if constexpr (Options::OPTIMIZATION_PROBLEMS_FATAL) {
                    throw log::error(this, "optimization reached maximum iterations (for ", input_storages.size(), " consumption goods)");
                } else {
                    log::warning(this, "optimization reached maximum iterations (for ", input_storages.size(), " consumption goods)");
                }
            } else if (optimizer.maxtime_reached()) {
                if constexpr (Options::DEBUGGING) {
                    debug_print_distribution();
                }
                model()->run()->event(EventType::OPTIMIZER_TIMEOUT, this);
                if constexpr (Options::OPTIMIZATION_PROBLEMS_FATAL) {
                    throw log::error(this, "optimization timed out (for ", input_storages.size(), " consumption goods)");
                } else {
                    log::warning(this, "optimization timed out (for ", input_storages.size(), " consumption goods)");
                }
            } else {
                log::warning(this, "optimization finished with ", optimizer.last_result_description());
            }
        }
    } catch (const optimization::failure& ex) {
        if constexpr (Options::DEBUGGING) {
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
    not_spent_budget_ += consumption_budget_;
    for (auto& input_storage : input_storages) {
        int const r = input_storage->id.index();
        input_storage->set_desired_used_flow(consumption[r]);
        input_storage->use_content(consumption[r]);
        region->add_consumption(consumption[r]);
        input_storage->iterate_consumption_and_production();
        previous_consumption_[r] = consumption[r];
        not_spent_budget_ -= consumption[r].get_value();
    }
    not_spent_budget_ = FlowValue(0.0);
}

void Consumer::iterate_expectation() { debug::assertstep(this, IterationStep::EXPECTATION); }

void Consumer::iterate_purchase() {
    debug::assertstep(this, IterationStep::PURCHASE);
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_purchase();
    }
}

void Consumer::iterate_investment() {
    debug::assertstep(this, IterationStep::INVESTMENT);
    // TODO INVEST
    for (const auto& is : input_storages) {
        is->purchasing_manager->iterate_investment();
    }
}

void Consumer::debug_print_details() const {
    if constexpr (Options::DEBUGGING) {
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
    if constexpr (Options::DEBUGGING) {
#pragma omp critical(output)
        {
            std::vector<double> grad_constraint(input_storages.size());
            std::vector<double> grad_objective(input_storages.size());

            std::cout << model()->run()->timeinfo() << ", " << name() << ": demand distribution for " << input_storages.size() << " inputs :\n";
            print_row("inequality value", inequality_constraint(optimizer_consumption_.data(), grad_constraint.data()));
            print_row("objective value", max_objective(optimizer_consumption_.data(), grad_objective.data()));
            print_row("current utility_", utility_);
            print_row("current total budget", consumption_budget_);
            print_row("substitution coefficient", inter_basket_substitution_coefficient_);
            std::cout << '\n';

            for (auto& input_storage : input_storages) {
                int const r = input_storage->id.index();
                std::cout << "    " << input_storage->name() << " :\n";
                print_row("grad (constraint)", grad_constraint[r]);
                print_row("grad (objective)", grad_objective[r]);
                print_row("share factor", share_factors_[r]);
                print_row("consumption quantity", invert_scaling_double_to_quantity(optimizer_consumption_[r], baseline_consumption_[r].get_quantity()));
                print_row("share of start cons. qty(%)", optimizer_consumption_[r]);
                print_row("consumption price", consumption_prices_[r]);
                print_row("consumption value",
                          invert_scaling_double_to_quantity(optimizer_consumption_[r], baseline_consumption_[r].get_quantity()) * consumption_prices_[r]);
                std::cout << '\n';
            }
        }
    }
}

auto Consumer::invert_scaling_double_to_quantity(double scaled_value, FlowQuantity scaling_quantity) -> FlowQuantity { return scaled_value * scaling_quantity; }
auto Consumer::invert_scaling_double_to_double(double scaled_value, FlowQuantity scaling_quantity) -> double {
    return scaled_value * to_float(scaling_quantity);
}
auto Consumer::scale_quantity_to_double(FlowQuantity quantity, FlowQuantity scaling_quantity) -> double { return to_float(quantity / scaling_quantity); }
auto Consumer::scale_double_to_double(double not_scaled_double, FlowQuantity scaling_quantity) -> double {
    return not_scaled_double / to_float(scaling_quantity);
}

}  // namespace acclimate
