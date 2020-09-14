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

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "ModelRun.h"
#include "acclimate.h"
#include "model/Model.h"
#include "model/PurchasingManager.h"
#include "model/Region.h"
#include "model/Storage.h"
#include "parameters.h"

static constexpr auto MAX_GRADIENT = 1e3;  // TODO: any need to modify?
static constexpr bool IGNORE_ROUNDOFFLIMITED = false;

namespace acclimate {
// TODO: in the long run abstract consumer with different implementations would be nice

Consumer::Consumer(Region* region_p) : EconomicAgent(region_p->model()->consumption_sector, region_p, EconomicAgent::Type::CONSUMER) {}

Consumer::Consumer(Region* region_p, float substitution_coefficient_p)
    : EconomicAgent(region_p->model()->consumption_sector, region_p, EconomicAgent::Type::CONSUMER) {
    substitution_coefficient = substitution_coefficient_p;
}

void Consumer::initialize() {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    share_factors = std::vector<FloatType>(input_storages.size());
    previous_prices = std::vector<FloatType>(input_storages.size());
    previous_consumption = std::vector<FloatType>(input_storages.size());

    desired_consumption = std::vector<FloatType>(input_storages.size());

    // initialize previous consumption as starting values, calculate budget
    budget = 0.0;
    for (std::size_t r = 0; r < input_storages.size(); ++r) {
        auto initial_consumption_flow = to_float(input_storages[r]->initial_used_flow_U_star().get_quantity());

        previous_consumption[r] = initial_consumption_flow;
        previous_prices[r] = 1;  // might be non-constant in the future, thus keeping this line
        budget += initial_consumption_flow * previous_prices[r];
    }
    // initialize share factors
    for (std::size_t r = 0; r < previous_prices.size(); ++r) {
        share_factors[r] = (previous_consumption[r] * previous_prices[r]) / budget;
    }
}

// TODO: define scaling functions after first tests of optimizer

// first draft of a definition of a CES utility function:

FloatType Consumer::CES_utility_function(std::vector<FloatType> consumption_demands) const {
    FloatType sum_over_goods = 0;
    if (consumption_demands.size() != share_factors.size())
        throw log::error(this, "number of goods and share factors does not match!");

    for (std::size_t r = 0; r < consumption_demands.size(); ++r) {
        sum_over_goods +=
            pow(consumption_demands[r], (substitution_coefficient - 1) / substitution_coefficient) * pow(share_factors[r], 1 / substitution_coefficient);
    }
    return sum_over_goods;  // outer exponent not relevant for optimization (at least for sigma >1)
}

FloatType Consumer::CES_marginal_utility(int index_of_good, std::vector<FloatType> consumption_demands) const {
    return pow(consumption_demands[index_of_good], (((substitution_coefficient - 1) / substitution_coefficient) - 1))
           * ((substitution_coefficient - 1) / substitution_coefficient) * pow(share_factors[index_of_good], 1 / substitution_coefficient);
}

// TODO: more complex budget function should be possible

FloatType Consumer::inequality_constraint(const double* x, double* grad) {
    FloatType consumption_cost = 0;
    for (std::size_t r = 0; r < consumption_prices.size(); ++r) {
        assert(!std::isnan(x[r]));
        consumption_cost += x[r] * consumption_prices[r];
        if (grad != nullptr) {
            grad[r] = CES_marginal_utility(r, desired_consumption);
            if constexpr (options::OPTIMIZATION_WARNINGS) {
                if (grad[r] > MAX_GRADIENT) {
                    log::warning(this, this->id(), ": large gradient of ", grad[r]);
                }
            }
        }
    }
    budget_gap = (budget - consumption_cost) * -1;  // inequality constraint checks for h(x) <=0, thus switch sign

    return budget_gap;
}

FloatType Consumer::max_objective(const double* x, double* grad) const {
    for (std::size_t r = 0; r < consumption_prices.size(); ++r) {
        // TODO:  maybe add scaling here, if needed
        assert(!std::isnan(x[r]));
        if (grad != nullptr) {
            grad[r] = CES_marginal_utility(r, desired_consumption);
            if constexpr (options::OPTIMIZATION_WARNINGS) {
                if (grad[r] > MAX_GRADIENT) {
                    log::warning(this, ": large gradient of ", grad[r]);
                }
            }
        }
    }
    return CES_utility_function(desired_consumption);
}

void Consumer::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    // get storage content of each good as maximal possible consumption and get prices from storage goods.
    // TODO: extend by alternative prices of most recent goods
    possible_consumption.clear();
    possible_consumption.reserve(input_storages.size());

    consumption_prices.clear();
    consumption_prices.reserve(input_storages.size());

    for (const auto& is : input_storages) {
        auto storage_content = is->get_possible_use_U_hat();
        // TODO: less "risky" data structure possible / needed?
        possible_consumption.push_back(to_float((storage_content.get_quantity())));
        consumption_prices.push_back((storage_content.get_price_float()));
    }

    // calculate maximal spending if all goods are consumed:
    FloatType maximum_spending = 0.0;
    for (std::size_t r = 0; r < possible_consumption.size(); ++r) {
        maximum_spending += possible_consumption[r] * consumption_prices[r];
    }
    // consumer can get all available goods, which is optimal assuming no saving incentives and positive marginal utility for all goods
    // (no "bads" i.e. commodities with negative utility like radioactive waste);

    if (budget - maximum_spending > 0) {
        desired_consumption = possible_consumption;  // TODO: needs to be refined, since storage emptying consumption highly unlikely
    } else {
        // consumer has to decide what goods he would like to get most. here based on utility function
        // start optimization at previous optimum, guarantees stability in undisturbed baseline
        // while potentially speeding up optimization in case of small price changes

        // check whether enough goods available &
        // adjust if price changes make previous consumption to expensive - assuming constant expenditure per good
        FloatType used_budget = 0;
        for (std::size_t r = 0; r < possible_consumption.size(); ++r) {
            desired_consumption[r] = std::min(possible_consumption[r], previous_consumption[r]);  // cap desired consumption at maximum possible
            used_budget += desired_consumption[r] * consumption_prices[r];
        }

        if (used_budget - budget < 0) {
            log::info(this, "consumption budget needs to be readjusted for ", desired_consumption.size() + 1, " consumption goods)");
            for (std::size_t r = 0; r < desired_consumption.size(); ++r) {
                FloatType overspending_factor = previous_prices[r] / consumption_prices[r];
                desired_consumption[r] = std::min(possible_consumption[r], desired_consumption[r] * overspending_factor);
                // TODO: seems a little arbitrary, but should use budget quite well (if price decreases, share of consumption is increased) -
                //  maybe price elasticity could be used here
            }
        }
        // utility optimization

        lower_bounds.clear();
        lower_bounds.reserve(desired_consumption.size());
        upper_bounds.clear();
        upper_bounds.reserve(desired_consumption.size());
        xtol_abs.clear();
        xtol_abs.reserve(desired_consumption.size());

        // set parameters
        for (std::size_t r = 0; r < desired_consumption.size(); ++r) {
            FloatType lower_limit = 0;
            FloatType upper_limit = possible_consumption[r];
            lower_bounds.push_back(lower_limit);
            upper_bounds.push_back(upper_limit);
            xtol_abs.push_back(FlowQuantity::precision);
        }

        try {
            optimization::Optimization opt(static_cast<nlopt_algorithm>(model()->parameters().optimization_algorithm),
                                           desired_consumption.size());  // TODO keep and only recreate when resize is needed

            opt.add_inequality_constraint(this, FlowValue::precision);
            opt.add_max_objective(this);

            opt.xtol(xtol_abs);
            opt.lower_bounds(lower_bounds);
            opt.upper_bounds(upper_bounds);
            opt.maxeval(model()->parameters().optimization_maxiter);
            opt.maxtime(model()->parameters().optimization_timeout);
            const auto res = opt.optimize(desired_consumption);
            if (!res && !opt.xtol_reached()) {
                if (opt.roundoff_limited()) {
                    if constexpr (!IGNORE_ROUNDOFFLIMITED) {
                        if constexpr (options::DEBUGGING) {
                            std::vector<double> desired_consumption_double = std::vector<double>(desired_consumption.size());
                            for (std::size_t r = 0; r < desired_consumption.size(); ++r) {
                                desired_consumption_double[r] = double(desired_consumption[r]);
                            }
                            print_distribution(desired_consumption_double);  // TODO: change to vector of flows?
                        }
                        model()->run()->event(EventType::OPTIMIZER_ROUNDOFF_LIMITED, this->region->model()->consumption_sector, nullptr, this);
                        if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                            throw log::error(this, "optimization is roundoff limited (for ", desired_consumption.size() + 1, " consumption goods)");
                        } else {
                            log::warning(this, "optimization is roundoff limited (for ", desired_consumption.size() + 1, " consumption goods)");
                        }
                    }
                }
            } else if (opt.maxtime_reached()) {
                if constexpr (options::DEBUGGING) {
                    std::vector<double> desired_consumption_double = std::vector<double>(desired_consumption.size());
                    for (std::size_t r = 0; r < desired_consumption.size(); ++r) {
                        desired_consumption_double[r] = double(desired_consumption[r]);
                    }
                    print_distribution(desired_consumption_double);  // TODO: change to vector of flows?
                }
                model()->run()->event(EventType::OPTIMIZER_TIMEOUT, this->region->model()->consumption_sector, nullptr, this);
                if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                    throw log::error(this, "optimization timed out (for ", desired_consumption.size(), " inputs)");
                } else {
                    log::warning(this, "optimization timed out (for ", desired_consumption.size() + 1, " inputs)");
                }

            } else {
                log::warning(this, "optimization finished with ", opt.last_result_description());
            }
            utility = (opt.optimized_value());  // TODO: use this anywhere?
        } catch (const optimization::failure& ex) {
            if constexpr (options::DEBUGGING) {
                std::vector<double> desired_consumption_double = std::vector<double>(desired_consumption.size());
                for (std::size_t r = 0; r < desired_consumption.size(); ++r) {
                    desired_consumption_double[r] = double(desired_consumption[r]);
                }
                print_distribution(desired_consumption_double);  // TODO: change to vector of flows?
            }
            // TODO maxiter limit is ok, the rest fatal
            if constexpr (options::OPTIMIZATION_PROBLEMS_FATAL) {
                throw log::error(this, "optimization failed, ", ex.what(), " (for ", desired_consumption.size(), " inputs)");
            } else {
                log::warning(this, "optimization failed, ", ex.what(), " (for ", desired_consumption.size() + 1, " inputs)");
            }
        }
    }

    // consume goods and adjust previous_consumption
    for (std::size_t r = 0; r < input_storages.size(); ++r) {
        const Flow consumption_flow = Flow((FlowQuantity(desired_consumption[r])), Price(consumption_prices[r]));
        // withdraw consumption from storage
        // adjust previous consumption vector
        previous_consumption[r] = desired_consumption[r];
        previous_prices[r] = consumption_prices[r];

        input_storages[r]->set_desired_used_flow_U_tilde(consumption_flow);
        input_storages[r]->use_content_S((consumption_flow));
        this->region->add_consumption_flow_Y((consumption_flow));
        input_storages[r]->iterate_consumption_and_production();
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
            for (std::size_t r = 0; r < consumption_demands.size(); ++r) {
                std::cout << "      " << this->id() << " :\n";
                print_row("grad", grad[r]);
                print_row("share factor", share_factors[r]);
                print_row("substitution coefficient", substitution_coefficient);
                print_row("consumption request", desired_consumption[r]);
                print_row("consumption price", consumption_prices[r]);
                print_row("current utility", utility);
                print_row("current total budget", budget);
                print_row("current spending for this good", consumption_prices[r] * desired_consumption[r]);
                print_row("budget gap", budget_gap);

                std::cout << '\n';
            }
        }
    }
}

}  // namespace acclimate
