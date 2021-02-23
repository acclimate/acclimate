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

#ifndef ACCLIMATE_CONSUMER_H
#define ACCLIMATE_CONSUMER_H

#include "acclimate.h"
#include "autodiff.h"
#include "model/EconomicAgent.h"

namespace acclimate {

class Region;

namespace optimization {
class Optimization;
}

class Consumer final : public EconomicAgent {
  private:
    bool utilitarian;

    size_t goods_num;
    size_t baskets_num;
    std::vector<std::vector<int>> goods_basket;

    // optimization parameters
    std::vector<double> optimizer_consumption;

    std::vector<FloatType> xtol_abs;
    std::vector<FloatType> xtol_abs_global;
    std::vector<FloatType> upper_bounds;
    std::vector<FloatType> lower_bounds;

    // consumption limits considered in optimization
    std::vector<Price> consumption_prices;  // prices to be considered in optimization
    std::vector<FloatType> desired_consumption;
    std::vector<Flow> previous_consumption;

    FlowValue not_spent_budget;  // TODO: introduce real saving possibility, for now just trying to improve numerical stability

    FloatType baseline_utility;  // baseline utility for scaling
    std::vector<Flow> baseline_consumption;

    // field to store utility
    FloatType utility;

    std::vector<FloatType> consumption_vector;  // vector to store actual, unscaled consumption
    // variables for (in)equality constraint, pre-allocated to increase efficiency
    FloatType consumption_budget;
    // variables for utiltiy function , pre-allocated to increase efficiency
    FloatType consumption_utility;
    FloatType basket_consumption_utility;
    autodiff::Value<FloatType> autodiff_consumption_utility = autodiff::Value<FloatType>(0, 0);
    autodiff::Value<FloatType> autodiff_basket_consumption_utility = autodiff::Value<FloatType>(0, 0);

  public:
    using EconomicAgent::input_storages;
    using EconomicAgent::region;

    FlowValue budget;
    FloatType inter_basket_substitution_coefficient;
    FloatType inter_basket_substitution_exponent;
    std::vector<FloatType> basket_share_factors;
    std::vector<FloatType> share_factors;
    std::vector<FloatType> intra_basket_substitution_coefficient;
    std::vector<FloatType> intra_basket_substitution_exponent;

  public:
    Consumer(id_t id_p,
             Region* region_p,
             FloatType inter_basket_substitution_coefficient_p,
             std::vector<std::vector<int>> consumer_baskets_p,
             std::vector<FloatType> intra_basket_substitution_coefficients_p);

    Consumer* as_consumer() override { return this; };
    const Consumer* as_consumer() const override { return this; };

    void initialize() override;

    void iterate_consumption_and_production() override;
    void iterate_expectation() override;
    void iterate_purchase() override;
    void iterate_investment() override;

    void debug_print_details() const override;
    void debug_print_distribution(const std::vector<FloatType>& demand_requests_D) const;

    // CES utility specific funtions TODO: check if replacing by abstract funtions suitable
    FloatType CES_utility_function(const std::vector<FloatType>& consumption_demands);
    FloatType CES_utility_function(const std::vector<Flow>& consumption_demands);

    // for autodiff test: some simple utility functions
    autodiff::Variable<FloatType> var_optimizer_consumption = autodiff::Variable<FloatType>(0, 0);

    autodiff::Value<FloatType> autodiffutility = autodiff::Value<FloatType>(0, 0);

    // for nested utility function
    autodiff::Value<FloatType> autodiff_nested_CES_utility_function(const autodiff::Variable<FloatType>& consumption_demands);

    // some stuff to enalbe local comparison of old consumer and utilitarian
    std::vector<FloatType> utilitarian_consumption_optimization();
    void utilitarian_consumption_execution(std::vector<FloatType> desired_consumption);
    std::vector<FloatType> utilitarian_consumption;
    FloatType local_optimal_utility;

    // functions for constrained optimization
    void consumption_optimize(optimization::Optimization& optimizer);
    FloatType inequality_constraint(const double* x, double* grad);
    FloatType equality_constraint(const double* x, double* grad);
    FloatType max_objective(const double* x, double* grad);
    // scaling function
    FloatType unscaled_demand(FloatType scaling_factor, int scaling_index) const;

    // getters and setters
    double get_utility() const { return utility; }
    double get_local_optimal_utility() const { return local_optimal_utility; }

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return EconomicAgent::observe<Observer, H>(o)  //
               && o.set(H::hash("utility"),
                        [this]() {  //
                            return get_utility();
                        })
               && o.set(H::hash("local_optimal_utility"),
                        [this]() {  //
                            return get_local_optimal_utility();
                        })
            //
            ;
    }
};
}  // namespace acclimate

#endif
