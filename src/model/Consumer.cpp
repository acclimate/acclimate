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
#include <optimization.h>

#include "ModelRun.h"
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

    share_factors.reserve(input_storages.size());
    previous_consumption.reserve(input_storages.size());
    baseline_consumption.reserve(input_storages.size());
    desired_consumption.reserve(input_storages.size());
    share_factors.reserve(input_storages.size());

    // initialize previous consumption as starting values, calculate budget
    budget = FlowValue(0.0);
    not_spent_budget = FlowValue(0.0);
    for (std::size_t r = 0; r < input_storages.size(); ++r) {
        Flow initial_consumption = input_storages[r]->initial_used_flow_U_star();
        previous_consumption.push_back(initial_consumption);
        budget += initial_consumption.get_value();
        baseline_consumption.push_back(initial_consumption);
        desired_consumption.push_back(initial_consumption);
    }

    // initialize share factors, potentiated already to speed up function calls
    for (std::size_t r = 0; r < input_storages.size(); ++r) {
        double budget_share = to_float(previous_consumption[r].get_value()) / to_float(budget);
        share_factors.push_back(pow(budget_share, 1 / substitution_coefficient));
    }
    baseline_utility = CES_utility_function(baseline_consumption);  // baseline utility for scaling
}

// TODO: more sophisticated scaling than normalizing with baseline utility might improve optimization?!


// definition of a simple linear utility function:
FloatType Consumer::linear_utility_function(std::vector<FlowQuantity> consumption_demands) const {
    FloatType sum_over_goods = 0.0;
    for (std::size_t r = 0; r < share_factors.size(); ++r) {
        sum_over_goods += to_float(consumption_demands[r]);
    }
    return sum_over_goods;
}

FloatType Consumer::linear_utility_function(std::vector<Flow> consumption_demands) const {
    FloatType sum_over_goods = 0.0;
    for (std::size_t r = 0; r < share_factors.size(); ++r) {
        sum_over_goods += to_float(consumption_demands[r].get_quantity());
    }
    return sum_over_goods;
}

FloatType Consumer::linear_marginal_utility(int index_of_good) const { return share_factors[index_of_good]; }

// definition of a simple CES utility function:
FloatType Consumer::CES_utility_function(std::vector<FlowQuantity> consumption_demands) const {
    FloatType sum_over_goods = 0.0;
    for (std::size_t r = 0; r < share_factors.size(); ++r) {
        sum_over_goods += pow(to_float(consumption_demands[r]), substitution_exponent) * share_factors[r];
    }
    return sum_over_goods;  // outer exponent not relevant for optimization (at least for sigma >1)
}

FloatType Consumer::CES_utility_function(std::vector<Flow> consumption_demands) const {
    FloatType sum_over_goods = 0.0;
    for (std::size_t r = 0; r < share_factors.size(); ++r) {
        sum_over_goods += pow(to_float(consumption_demands[r].get_quantity()), substitution_exponent) * share_factors[r];
    }
    return sum_over_goods;  // outer exponent not relevant for optimization (at least for sigma >1)
}

FloatType Consumer::CES_marginal_utility(int index_of_good, double consumption_demand) const {
    return pow(consumption_demand, substitution_exponent - 1) * substitution_exponent * share_factors[index_of_good];
}

// TODO: more complex budget function might be useful, e.g. opportunity for saving etc.

FloatType Consumer::equality_constraint(const double* x, double* grad) {
    auto consumption_cost = FlowValue(0.0);
    for (std::size_t r = 0; r < desired_consumption.size(); ++r) {
        assert(!std::isnan(x[r]));
        consumption_cost += FlowValue(unscaled_demand(x[r], r) * to_float(consumption_prices[r]));
        if (grad != nullptr) {
            grad[r] = CES_marginal_utility(r, unscaled_demand(x[r], r));
            if constexpr (options::OPTIMIZATION_WARNINGS) {
                if (grad[r] > MAX_GRADIENT) {
                    log::warning(this, this->id(), ": large gradient of ", grad[r]);
                }
            }
        }
    }
    double available_budget = to_float(budget + not_spent_budget);
    double consumption_expenditure = to_float(consumption_cost);
    double expenditure_ratio = consumption_expenditure / available_budget;
    return expenditure_ratio - 1;  // -1* to_float(budget + not_spent_budget - consumption_cost);
}

FloatType Consumer::max_objective(const double* x, double* grad) const {
    std::vector<FlowQuantity> consumption = std::vector<FlowQuantity>(desired_consumption.size());
    double sum_x = 0.0;
    for (std::size_t r = 0; r < desired_consumption.size(); ++r) {
        // TODO:  maybe add scaling here, if needed
        assert(!std::isnan(x[r]));
        if (grad != nullptr) {
            grad[r] = CES_marginal_utility(r, unscaled_demand(x[r], r));
            if (options::OPTIMIZATION_WARNINGS) {
                if (grad[r] > MAX_GRADIENT) {
                    log::warning(this, ": large gradient of ", grad[r]);
                }
            }
        }
        consumption[r] = FlowQuantity(unscaled_demand(x[r], r));
        sum_x += x[r];
    }
    return CES_utility_function(consumption) / baseline_utility;
}

void Consumer::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    if (utilitarian) {
        // get storage content of each good as maximal possible consumption and get prices from storage goods.
        // TODO: extend by alternative prices of most recent goods
        std::vector<FlowQuantity> possible_consumption = std::vector<FlowQuantity>(input_storages.size());
        std::vector<Price> current_prices = std::vector<Price>(input_storages.size());
        consumption_prices = std::vector<Price>(input_storages.size());

        for (std::size_t r = 0; r < input_storages.size(); ++r) {
            Flow possible_consumption_flow = input_storages[r]->get_possible_use_U_hat();
            possible_consumption[r] = (possible_consumption_flow.get_quantity());
            Price current_price = round(input_storages[r]->current_input_flow_I().get_price());
            consumption_prices[r] = (possible_consumption_flow.get_price());
            current_prices.push_back(current_price);  // using this would be giving the consumer some "foresight" by optimizing using the current price of
            // new goods, not just the average price of storage goods
        }

        // consumer has to decide what goods he would like to get most. here based on utility function
        // start optimization at baseline consumption, guarantees stability in undisturbed baseline
        // while potentially speeding up optimization in case of small price changes

        for (std::size_t r = 0; r < input_storages.size(); ++r) {
            desired_consumption[r] = (Flow(baseline_consumption[r].get_quantity(), consumption_prices[r]));
        }

        // adjust if price changes make previous consumption to expensive - scaling with elasticity
        for (std::size_t r = 0; r < desired_consumption.size(); ++r) {
            desired_consumption[r] =
                Flow(FlowQuantity(std::min(possible_consumption[r],
                                           desired_consumption[r].get_quantity() * baseline_consumption[r].get_price_float()
                                               * pow(to_float(consumption_prices[r]), input_storages[r]->parameters().consumption_price_elasticity))),
                     consumption_prices[r]);
            // TODO: more sophisticated use of price elasticity
        }

        // utility optimization

        // set parameters
        xtol_abs = std::vector(desired_consumption.size(), FlowQuantity::precision);
        optimizer_consumption = std::vector<FloatType>(desired_consumption.size(), 1.0);  // use normalized variable for optimization to improve?! performance
        desired_quantity = std::vector<FloatType>(desired_consumption.size());
        std::vector<FloatType> upper_bounds = std::vector<FloatType>(desired_consumption.size());
        std::vector<FloatType> lower_bounds = std::vector<FloatType>(desired_consumption.size());

        for (std::size_t r = 0; r < desired_consumption.size(); ++r) {
            desired_quantity[r] = to_float(desired_consumption[r].get_quantity());  // to_float((desired_consumption[r].get_quantity()));
            upper_bounds[r] = (to_float(possible_consumption[r])) / desired_quantity[r];
            lower_bounds[r] = (0.0);
        }

        // start optimizer
        try {
            optimization::Optimization opt(static_cast<nlopt_algorithm>(model()->parameters().utility_optimization_algorithm),
                                           optimizer_consumption.size());  // TODO keep and only recreate when resize is needed

            opt.add_equality_constraint(this, FlowValue::precision);
            opt.add_max_objective(this);
            opt.xtol(xtol_abs);  // ignoring this to avoid problems with quite small gradient
            opt.lower_bounds(lower_bounds);
            opt.upper_bounds(upper_bounds);
            opt.maxeval(model()->parameters().optimization_maxiter);
            opt.maxtime(model()->parameters().optimization_timeout);
            std::cout << "distribution before optimization \n";
            for (std::size_t r = 0; r < optimizer_consumption.size(); ++r) {
                std::cout << optimizer_consumption[r];
            }
            const auto res = opt.optimize(optimizer_consumption);
            std::cout << "\n distribution after optimization \n";
            for (std::size_t r = 0; r < optimizer_consumption.size(); ++r) {
                std::cout << optimizer_consumption[r];
                std::cout << "\t";
            }
            std::cout << "\n";
            if (!res && !opt.xtol_reached()) {
                if (opt.roundoff_limited()) {
                    if constexpr (!IGNORE_ROUNDOFFLIMITED) {
                        if constexpr (options::DEBUGGING) {
                            print_distribution(optimizer_consumption);
                        }
                        model()->run()->event(EventType::OPTIMIZER_ROUNDOFF_LIMITED, this->region->model()->consumption_sector, nullptr, this);
                        if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                            throw log::error(this, "optimization is roundoff limited (for ", optimizer_consumption.size(), " consumption goods)");
                        } else {
                            log::warning(this, "optimization is roundoff limited (for ", optimizer_consumption.size(), " consumption goods)");
                        }
                    }
                }
            } else if (opt.maxtime_reached()) {
                if constexpr (options::DEBUGGING) {
                    print_distribution(optimizer_consumption);
                }
                model()->run()->event(EventType::OPTIMIZER_TIMEOUT, this->region->model()->consumption_sector, nullptr, this);
                if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                    throw log::error(this, "optimization timed out (for ", optimizer_consumption.size(), " inputs)");
                } else {
                    log::warning(this, "optimization timed out (for ", optimizer_consumption.size(), " inputs)");
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
                throw log::error(this, "optimization failed, ", ex.what(), " (for ", optimizer_consumption.size(), " inputs)");
            } else {
                log::warning(this, "optimization failed, ", ex.what(), " (for ", optimizer_consumption.size(), " inputs)");
            }
        }

        // initialize available budget
        not_spent_budget += budget;

        // set consumption and previozs consumption

        for (std::size_t r = 0; r < input_storages.size(); ++r) {
            desired_consumption[r] = (Flow(FlowQuantity(unscaled_demand(optimizer_consumption[r], r)), consumption_prices[r]));

            // withdraw consumption from storage
            Flow used_flow_U = desired_consumption[r];
            input_storages[r]->set_desired_used_flow_U_tilde(round(desired_consumption[r]));
            input_storages[r]->use_content_S(round(used_flow_U));
            region->add_consumption_flow_Y(round(used_flow_U));
            // consume goods
            input_storages[r]->iterate_consumption_and_production();
            // adjust previous consumption
            previous_consumption[r] = used_flow_U;
            // adjust non-spent budget
            not_spent_budget -= used_flow_U.get_value();
        }

    } else {
        // old consumer to be used for comparison
        // utility calculated to compare
        desired_consumption = std::vector<Flow>();
        for (const auto& is : input_storages) {
            Flow possible_used_flow_U_hat = is->get_possible_use_U_hat();  // Price(U_hat) = Price of used flow
            Price reservation_price(0.0);
            if (possible_used_flow_U_hat.get_quantity() > 0.0) {
                // we have to purchase with the average price of input and storage
                reservation_price = possible_used_flow_U_hat.get_price();
            } else {                                                         // possible used flow is zero
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
            desired_consumption.push_back(used_flow_U);
        }
    }
    utility = CES_utility_function(desired_consumption) / baseline_utility;
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
            std::cout << model()->run()->timeinfo() << ", " << id() << ": demand distribution for " << consumption_demands.size() << " inputs :\n";
            std::vector<FloatType> grad(consumption_demands.size());
            max_objective(&consumption_demands[0], &grad[0]);
            for (std::size_t r = 0; r < consumption_demands.size(); ++r) {
                std::cout << "      " << this->id() << " :\n";
                print_row("grad", grad[r]);
                print_row("share factor", share_factors[r]);
                print_row("substitution coefficient", substitution_coefficient);
                print_row("consumption request", unscaled_demand(optimizer_consumption[r], r));
                print_row("starting value consumption request", desired_quantity[r]);
                print_row("consumption price", consumption_prices[r]);
                print_row("current utility", utility);
                print_row("current total budget", budget);
                print_row("current spending for this good", optimizer_consumption[r] * desired_quantity[r] * consumption_prices[r]);

                std::cout << '\n';
            }
        }
    }
}
FloatType Consumer::unscaled_demand(double d, int scaling_index) const { return d * desired_quantity[scaling_index]; }

}  // namespace acclimate
