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

#include "model/EconomicAgent.h"

namespace acclimate {

class Region;

class Consumer : public EconomicAgent {
  private:
    using EconomicAgent::forcing_;

  private:
    bool utilitarian;

    // optimization parameters
    std::vector<FloatType> optimizer_consumption;
    std::vector<double> lower_bounds;
    std::vector<double> upper_bounds;
    std::vector<double> xtol_abs;

    std::vector<FlowQuantity> possible_consumption;  // consumption limits considered in optimization
    std::vector<Price> consumption_prices;           // prices to be considered in optimization
    std::vector<Price> current_prices;               // current prices to enable optimization with some foresight
    std::vector<Flow> desired_consumption;
    std::vector<FlowQuantity> previous_consumption;
    std::vector<Price> previous_prices;

    FlowValue not_spent_budget;  // TODO: introduce real saving possibility, for now just trying to improve numerical stability

    double baseline_utility;  // baseline utility for scaling
    std::vector<Flow> baseline_consumption;

    // field to store utility
    double utility;

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
    void print_details() const override;

    // CES utility specific funtions TODO: check if replacing by abstract funtions suitable
    FloatType CES_utility_function(std::vector<FlowQuantity> consumption_demands) const;
    FloatType CES_utility_function(std::vector<Flow> consumption_demands) const;
    FloatType CES_marginal_utility(int index_of_good, FlowQuantity consumption_demand) const;
    FloatType CES_marginal_utility(int index_of_good, double consumption_demand) const;

    // some simple utility functions
    FloatType linear_utility_function(std::vector<FlowQuantity> consumption_demands) const;
    FloatType linear_utility_function(std::vector<Flow> consumption_demands) const;
    FloatType linear_marginal_utility(int index_of_good, double consumption_demand) const;

    // functions for constrained optimization
    FloatType equality_constraint(const double* x, double* grad);
    FloatType max_objective(const double* x, double* grad) const;
    void print_distribution(const std::vector<double>& demand_requests_D) const;

    // some simple function
    std::vector<FlowQuantity> get_quantity_vector(std::vector<Flow> flow);

    // getters and setters
    double get_utility() const;
};
}  // namespace acclimate

#endif
