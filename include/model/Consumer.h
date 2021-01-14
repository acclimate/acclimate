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

#include <settingsnode.h>

#include "autodiff.h"
#include "model/EconomicAgent.h"

namespace acclimate {

class Region;

class Consumer : public EconomicAgent {
  private:
    using EconomicAgent::forcing_;

  private:
    bool utilitarian;

    size_t goods_num = 1;

    // optimization parameters
    std::vector<double> optimizer_consumption;
    size_t size = 4;

    std::vector<double> xtol_abs;
    std::vector<double> xtol_abs_global;
    std::vector<FloatType> upper_bounds;
    std::vector<FloatType> lower_bounds;
    std::vector<FloatType> global_upper_bounds;

    // consumption limits considered in optimization
    std::vector<Price> consumption_prices;  // prices to be considered in optimization
    std::vector<FloatType> desired_consumption;
    std::vector<Flow> previous_consumption;

    FlowValue not_spent_budget;  // TODO: introduce real saving possibility, for now just trying to improve numerical stability

    double baseline_utility;  // baseline utility for scaling
    std::vector<Flow> baseline_consumption;

    // field to store utility
    double utility;

    // variables for equality constraint, pre-allocated to increase efficiency
    FloatType consumption_cost;
    FloatType available_budget;
    FloatType expenditure_ratio;
    // variables for max objective , pre-allocated to increase efficiency
    autodiff::Variable<FloatType> var_optimizer_consumption{0, goods_num, goods_num, 0.0};

  public:
    using EconomicAgent::input_storages;
    using EconomicAgent::region;

    FlowValue budget;

    std::vector<double> share_factors;
    double substitution_coefficient;
    double substitution_exponent;

  public:
    Consumer* as_consumer() override { return this; };
    explicit Consumer(Region* region_p);

    explicit Consumer(Region* region_p, float substitution_coefficient);

    void initialize() override;

    void iterate_consumption_and_production() override;
    void iterate_expectation() override;
    void iterate_purchase() override;
    void iterate_investment() override;
    using EconomicAgent::id;
    using EconomicAgent::model;

    // DEBUG
    bool verbose_consumer = true;
    void print_details() const override;

    // CES utility specific funtions TODO: check if replacing by abstract funtions suitable
    FloatType CES_utility_function(std::vector<FloatType> consumption_demands) const;
    FloatType CES_utility_function(std::vector<Flow> consumption_demands) const;
    FloatType CES_marginal_utility(int index_of_good, double consumption_demand) const;

    // for autodiff test: some simple utility functions
    bool utility_autodiff = true;
    autodiff::Value<FloatType> autodiffutilityautodiffutility{goods_num, 0.0};
    autodiff::Value<FloatType> const autodiff_CES_utility_function(autodiff::Variable<FloatType> consumption_demands) const;

    // some stuff to enalbe local comparison of old consumer and utilitarian
    std::vector<FloatType> utilitarian_consumption_optimization();
    void utilitarian_consumption_execution(std::vector<FloatType> desired_consumption);
    std::vector<FloatType> utilitarian_consumption;
    FloatType local_optimal_utility;

    // functions for constrained optimization
    void consumption_optimize(optimization::Optimization& optimizer);
    FloatType equality_constraint(const double* x, double* grad);
    FloatType max_objective(const double* x, double* grad) const;
    void print_distribution(const std::vector<double>& demand_requests_D) const;
    // scaling function
    FloatType unscaled_demand(double d, int scaling_index) const;

    // getters and setters
    double get_utility() const;
    double get_local_optimal_utility() const;
};
}  // namespace acclimate

#endif
