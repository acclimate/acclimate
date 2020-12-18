/*
  Copyright (C) 2014-2020 Sven Willner<sven.willner@pik-potsdam.de>
                          Christian Otto<christian.otto@pik-potsdam.de>

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
  along with Acclimate.  If not, see<http://www.gnu.org/licenses/>.
*/

#include "model/Consumer.h"

#include <optimization.h>

#include "ModelRun.h"
#include "autodiff.h"
#include "model/Model.h"

static constexpr auto MAX_GRADIENT = 1e3;  // TODO: any need to modify?
static constexpr bool IGNORE_ROUNDOFFLIMITED = false;

namespace acclimate {
// TODO: in the long run abstract consumer with different implementations would be nice

Consumer::Consumer(Region* region_p) : EconomicAgent(region_p->model()->consumption_sector, region_p, EconomicAgent::Type::CONSUMER) {}

Consumer::Consumer(Region* region_p, float substitution_coefficient_p)
    : EconomicAgent(region_p->model()->consumption_sector, region_p, EconomicAgent::Type::CONSUMER) {
    substitution_coefficient = substitution_coefficient_p;
    substitution_exponent = (substitution_coefficient - 1) / substitution_coefficient;
}

void Consumer::initialize() {
    utilitarian = model()->parameters_writable().consumer_utilitarian;

    debug::assertstep(this, IterationStep::INITIALIZATION);

    goods_num = input_storages.size();

    share_factors.reserve(goods_num);
    previous_consumption.reserve(goods_num);
    baseline_consumption.reserve(goods_num);
    desired_consumption.reserve(goods_num);

    // initialize previous consumption as starting values, calculate budget

    budget = FlowValue(0.0);
    not_spent_budget = FlowValue(0.0);
    Flow initial_consumption = input_storages[0]->initial_used_flow_U_star();
    for (std::size_t r = 0; r < goods_num; ++r) {
        initial_consumption = input_storages[r]->initial_used_flow_U_star();
        previous_consumption.push_back(initial_consumption);
        budget += initial_consumption.get_value();
        baseline_consumption.push_back(initial_consumption);
        desired_consumption.push_back(to_float(initial_consumption.get_quantity()));
    }

    // initialize share factors, potentiated already to speed up function calls
    for (std::size_t r = 0; r < goods_num; ++r) {
        double budget_share = to_float(previous_consumption[r].get_value()) / to_float(budget);
        share_factors.push_back(pow(budget_share, 1 / substitution_coefficient));
    }
    if (utility_autodiff)
        baseline_utility = linear_utility_function(baseline_consumption);  // baseline utility for scaling
    else
        baseline_utility = CES_utility_function(baseline_consumption);
}

// TODO: more sophisticated scaling than normalizing with baseline utility might improve optimization?!

// test with automatic differentiation

FloatType Consumer::linear_utility_function(std::vector<FloatType> consumption_demands) const {
    FloatType utility;

    for (std::size_t r = 0; r < goods_num; ++r) {
        utility += consumption_demands[r];
    }

    return utility;
}

FloatType Consumer::linear_utility_function(std::vector<Flow> consumption_demands) const {
    FloatType utility;
    for (std::size_t r = 0; r < goods_num; ++r) {
        utility += to_float(consumption_demands[r].get_quantity());
    }
    return utility;
}

autodiff::Value<FloatType> const Consumer::autodiff_linear_utility_function(autodiff::Variable<FloatType> consumption_demands) const {
    autodiff::Value<FloatType> utility = {goods_num, 0.0};

    for (std::size_t r = 0; r < goods_num; ++r) {
        utility += (consumption_demands[r]);
    }
    return utility;
}

// definition of a simple CES utility function:
FloatType Consumer::CES_utility_function(std::vector<FloatType> consumption_demands) const {
    FloatType utility = 0.0;
    for (std::size_t r = 0; r < goods_num; ++r) {
        utility += pow((consumption_demands[r]), substitution_exponent) * share_factors[r];
    }
    return utility;  // outer exponent not relevant for optimization (at least for sigma >1)
}

FloatType Consumer::CES_utility_function(std::vector<Flow> consumption_demands) const {
    FloatType utility = 0.0;
    for (std::size_t r = 0; r < goods_num; ++r) {
        utility += pow(to_float(consumption_demands[r].get_quantity()), substitution_exponent) * share_factors[r];
    }
    return utility;  // outer exponent not relevant for optimization (at least for sigma >1)
}

FloatType Consumer::CES_marginal_utility(int index_of_good, double consumption_demand) const {
    return pow(consumption_demand, substitution_exponent - 1) * substitution_exponent * share_factors[index_of_good];
}

// TODO: more complex budget function might be useful, e.g. opportunity for saving etc.

FloatType Consumer::equality_constraint(const double* x, double* grad) {
    consumption_cost = 0.0;
    for (std::size_t r = 0; r < goods_num; ++r) {
        assert(!std::isnan(x[r]));
        consumption_cost += (unscaled_demand(x[r], r) * to_float(consumption_prices[r]));
    }
    if (grad != nullptr) {
        if (utility_autodiff)
            for (std::size_t r = 0; r < goods_num; ++r)
                grad[r] = 1;  // TODO: redesign autodiffutility as global variable such that derivative can be called upon from here, i.e. grad[r] =
                              // autodiffutility.derivative()[r]
        else
            for (std::size_t r = 0; r < goods_num; ++r) grad[r] = CES_marginal_utility(r, unscaled_demand(x[r], r));
        if (options::OPTIMIZATION_WARNINGS) {
            for (std::size_t r = 0; r < goods_num; ++r) {
                if (grad[r] > MAX_GRADIENT) {
                    log::warning(this, ": large gradient of ", grad[r]);
                }
            }
        }
    }
    available_budget = to_float(budget + not_spent_budget);
    expenditure_ratio = consumption_cost / available_budget;
    if (verbose_consumer)
        std::cout << "expenditure ratio \t" << expenditure_ratio << "\n";
    return expenditure_ratio - 1;
}

FloatType Consumer::max_objective(const double* x, double* grad) const {
    std::vector<FloatType> consumption_vector;
    consumption_vector.reserve(goods_num);
    for (std::size_t r = 0; r < goods_num; ++r) {
        assert(!std::isnan(x[r]));
        consumption_vector.push_back(unscaled_demand(x[r], r));
    }
    if (utility_autodiff) {
        autodiff::Variable<FloatType> var_optimizer_consumption = autodiff::Variable<FloatType>(0, goods_num, goods_num, 0);
        var_optimizer_consumption = optimizer_consumption;
        autodiff::Value<FloatType> autodiffutility = autodiff_linear_utility_function(var_optimizer_consumption);
        if (grad != nullptr) {
            std::copy(&autodiffutility.derivative()[0], &autodiffutility.derivative()[goods_num], grad);
            if (options::OPTIMIZATION_WARNINGS) {
                for (std::size_t r = 0; r < goods_num; ++r) {
                    if (grad[r] > MAX_GRADIENT) {
                        log::warning(this, ": large gradient of ", grad[r]);
                    }
                }
            }
        }
        return autodiffutility.value();
    } else {
        FloatType CES_utility = CES_utility_function(consumption_vector);
        for (std::size_t r = 0; r < desired_consumption.size(); ++r) {
            if (grad != nullptr) {
                grad[r] = CES_marginal_utility(r, consumption_vector[r]);
                if (options::OPTIMIZATION_WARNINGS) {
                    if (grad[r] > MAX_GRADIENT) {
                        log::warning(this, ": large gradient of ", grad[r]);
                    }
                }
            }
        }
        return CES_utility;
    }
}

void Consumer::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    if (utilitarian) {
        // get storage content of each good as maximal possible consumption and get prices from storage goods.
        // TODO: extend by alternative prices of most recent goods
        std::vector<Flow> possible_consumption;
        std::vector<Price> current_prices;

        for (std::size_t r = 0; r < goods_num; ++r) {
            possible_consumption.push_back(input_storages[r]->get_possible_use_U_hat());
            Price current_price = round(input_storages[r]->current_input_flow_I().get_price());
            consumption_prices.push_back(possible_consumption[r].get_price());
            current_prices.push_back(current_price);  // using this would be giving the consumer some "foresight" by optimizing using the current price of
            // new goods, not just the average price of storage goo
        }
        // adjust possible consumption such that feasibile under budget constraint:
        FlowValue possible_consumption_spending = FlowValue(0.0);
        for (std::size_t r = 0; r < goods_num; ++r) possible_consumption_spending += possible_consumption[r].get_value();
        FloatType budget_to_capacity_ratio = (budget + not_spent_budget) / possible_consumption_spending;
        std::vector<FloatType> feasibile_consumption;
        for (std::size_t r = 0; r < goods_num; ++r)
            feasibile_consumption.push_back(to_float(possible_consumption[r].get_quantity()) * budget_to_capacity_ratio);

        // consumer has to decide what goods he would like to get most. here based on utility function
        // start optimization at baseline consumption, guarantees stability in undisturbed baseline
        // while potentially speeding up optimization in case of small price changes

        for (std::size_t r = 0; r < goods_num; ++r) {
            desired_consumption[r] = to_float(baseline_consumption[r].get_quantity());
        }
        // adjust if price changes make baseline consumption to expensive - scaling with elasticity
        for (std::size_t r = 0; r < goods_num; ++r) {
            desired_consumption[r] *=
                baseline_consumption[r].get_price_float() * pow(to_float(consumption_prices[r]), input_storages[r]->parameters().consumption_price_elasticity);
            // TODO: more sophisticated use of price elasticity
        }

        // utility optimization
        // set parameters
        xtol_abs = std::vector(goods_num, FlowQuantity::precision);
        optimizer_consumption = std::vector<double>(goods_num, 1.0);  // use normalized variable for optimization to improve?! performance

        upper_bounds = std::vector<FloatType>(goods_num);
        lower_bounds = std::vector<FloatType>(goods_num);
        // set bounds
        for (std::size_t r = 0; r < goods_num; ++r) {
            lower_bounds[r] = (0.0);
            upper_bounds[r] = to_float(possible_consumption[r].get_quantity()) / desired_consumption[r];
            if (verbose_consumer)
                std::cout << "upper bound: " << upper_bounds[r] << "\n";
        }

        // start optimizer
        try {
            optimization::Optimization opt(static_cast<nlopt_algorithm>(model()->parameters().utility_optimization_algorithm),
                                           goods_num);  // TODO keep and only recreate when resize is needed
            opt.add_equality_constraint(this, 0.001);
            opt.add_max_objective(this);
            opt.xtol(xtol_abs);
            opt.lower_bounds(lower_bounds);
            opt.upper_bounds(upper_bounds);
            opt.maxeval(model()->parameters().optimization_maxiter);
            opt.maxtime(model()->parameters().optimization_timeout);
            if (verbose_consumer) {
                std::cout << "\n distribution before optimization \n";
                for (std::size_t r = 0; r < goods_num; ++r) {
                    std::cout << optimizer_consumption[r];
                    std::cout << "\t";
                }
                std::cout << "\n";
            }
            const auto res = opt.optimize(optimizer_consumption);
            if (verbose_consumer) {
                std::cout << "\n distribution after optimization \n";
                for (std::size_t r = 0; r < goods_num; ++r) {
                    std::cout << optimizer_consumption[r];
                    std::cout << "\t";
                }
                std::cout << "\n";
            }
            if (!res && !opt.xtol_reached()) {
                if (opt.roundoff_limited()) {
                    if constexpr (!IGNORE_ROUNDOFFLIMITED) {
                        if constexpr (options::DEBUGGING) {
                            print_distribution(optimizer_consumption);
                        }
                        model()->run()->event(EventType::OPTIMIZER_ROUNDOFF_LIMITED, this->region->model()->consumption_sector, nullptr, this);
                        if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                            throw log::error(this, "optimization is roundoff limited (for ", goods_num, " consumption goods)");
                        } else {
                            log::warning(this, "optimization is roundoff limited (for ", goods_num, " consumption goods)");
                        }
                    }
                }
            } else if (opt.maxtime_reached()) {
                if constexpr (options::DEBUGGING) {
                    print_distribution(optimizer_consumption);
                }
                model()->run()->event(EventType::OPTIMIZER_TIMEOUT, this->region->model()->consumption_sector, nullptr, this);
                if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                    throw log::error(this, "optimization timed out (for ", goods_num, " inputs)");
                } else {
                    log::warning(this, "optimization timed out (for ", goods_num, " inputs)");
                }

            } else {
                log::warning(this, "optimization finished with ", opt.last_result_description());
            }
        } catch (const optimization::failure& ex) {
            if constexpr (options::DEBUGGING) {
                print_distribution(optimizer_consumption);  // TODO: change to vector of flows?
            }
            // TODO maxiter limit is ok, the rest fatal
            if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                throw log::error(this, "optimization failed, ", ex.what(), " (for ", goods_num, " inputs)");
            } else {
                log::warning(this, "optimization failed, ", ex.what(), " (for ", goods_num, " inputs)");
            }
        }

        // initialize available budget
        not_spent_budget += budget;

        // set consumption and previous consumption

        for (std::size_t r = 0; r < goods_num; ++r) {
            Flow desired_consumption_flow = (Flow(FlowQuantity(unscaled_demand(optimizer_consumption[r], r)), consumption_prices[r]));
            desired_consumption[r] = (to_float(desired_consumption_flow.get_quantity()));
            // withdraw consumption from storage
            input_storages[r]->set_desired_used_flow_U_tilde(round(desired_consumption_flow));
            input_storages[r]->use_content_S(round(desired_consumption_flow));
            region->add_consumption_flow_Y(round(desired_consumption_flow));
            // consume goods
            input_storages[r]->iterate_consumption_and_production();
            // adjust previous consumption
            previous_consumption[r] = desired_consumption_flow;
            // adjust non-spent budget
            not_spent_budget -= desired_consumption_flow.get_value();
        }
        if (verbose_consumer)
            std::cout << "not-spent budget: \t" << not_spent_budget << "\n";
    } else {
        // old consumer to be used for comparison
        // utility calculated to compare
        desired_consumption = std::vector<FloatType>();
        for (const auto& is : input_storages) {
            Flow possible_used_flow_U_hat = is->get_possible_use_U_hat();  // Price(U_hat) = Price of used flow
            Price reservation_price(0.0);
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

            const Flow desired_used_flow_U_tilde = Flow(round(is->initial_input_flow_I_star().get_quantity() * forcing_
                                                              * pow(reservation_price / Price(1.0), is->parameters().consumption_price_elasticity)),
                                                        reservation_price);
            const Flow used_flow_U =
                Flow(FlowQuantity(std::min(desired_used_flow_U_tilde.get_quantity(), possible_used_flow_U_hat.get_quantity())), reservation_price);
            is->set_desired_used_flow_U_tilde(desired_used_flow_U_tilde);
            is->use_content_S(round(used_flow_U));
            region->add_consumption_flow_Y(round(used_flow_U));
            is->iterate_consumption_and_production();
            desired_consumption.push_back(to_float(used_flow_U.get_quantity()));
        }
    }
    utility = utility_autodiff ? linear_utility_function(desired_consumption) : CES_utility_function(desired_consumption);
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

std::vector<FlowQuantity> get_quantity_vector(std::vector<Flow>& flow) {
    std::vector<FlowQuantity> quantity;
    for (auto& flow_r : flow) {
        quantity.push_back(flow_r.get_quantity());
    }
    return quantity;
}

std::vector<FloatType> get_float_vector(std::vector<FlowQuantity> flow_quantity) {
    std::vector<FloatType> floats;
    for (auto& flow_r : flow_quantity) {
        floats.push_back(to_float(flow_r));
    }
    return floats;
}

double Consumer::get_utility() const { return utility; }

void Consumer::print_details() const {
    // TODO: adjust this to give info on U(x), budget, starting values, etc.
    if constexpr (options::DEBUGGING) {
        log::info(this);
        for (const auto& is : input_storages) {
            is->purchasing_manager->print_details();
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

void Consumer::print_distribution(const std::vector<double>& consumption_demands) const {
    if constexpr (options::DEBUGGING) {
#pragma omp critical(output)
        {
            std::cout << model()->run()->timeinfo() << ", " << id() << ": demand distribution for " << goods_num << " inputs :\n";
            std::vector<FloatType> grad(goods_num);
            max_objective(&consumption_demands[0], &grad[0]);
            for (std::size_t r = 0; r < goods_num; ++r) {
                std::cout << "      " << this->id() << " :\n";
                print_row("grad", grad[r]);
                print_row("share factor", share_factors[r]);
                print_row("substitution coefficient", substitution_coefficient);
                print_row("consumption request", unscaled_demand(optimizer_consumption[r], r));
                print_row("starting value consumption request", desired_consumption[r]);
                print_row("consumption price", consumption_prices[r]);
                print_row("current utility", utility / baseline_utility);
                print_row("current total budget", budget);
                print_row("current spending for this good", optimizer_consumption[r] * desired_consumption[r] * consumption_prices[r]);

                std::cout << '\n';
            }
        }
    }
}
FloatType Consumer::unscaled_demand(double d, int scaling_index) const { return d * desired_consumption[scaling_index]; }

}  // namespace acclimate
