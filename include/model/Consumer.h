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

    // opitmization parameters
    std::vector<double> upper_bounds;
    std::vector<double> lower_bounds;
    std::vector<double> xtol_abs;

    // vectors to store desired actual, and previous consumption quantities as floats
    std::vector<FloatType> possible_consumption;  // consumption limits considered in optimization
    std::vector<FloatType> consumption_prices;    // prices to be considered in optimization
    std::vector<FloatType> desired_consumption;
    std::vector<FloatType> previous_consumption;
    std::vector<FloatType> previous_prices;

    // field to store utility
    double utility;
    FloatType budget_gap;

    // starting values
    std::vector<FloatType> inital_prices;
    std::vector<FloatType> initial_consumption;

  public:
    using EconomicAgent::input_storages;
    using EconomicAgent::region;
    float budget;  // TODO: less crude way of introducing budget?!
    std::vector<FloatType> share_factors;
    FloatType substitution_coefficient{};

  public:
    Consumer* as_consumer() override { return this; };
    explicit Consumer(Region* region_p);  // TODO: replace constructor in other classes

    explicit Consumer(Region* region_p, float substitution_coefficient);

    void initialize();

    void iterate_consumption_and_production() override;
    void iterate_expectation() override;
    void iterate_purchase() override;
    void iterate_investment() override;
    using EconomicAgent::id;
    using EconomicAgent::model;

    // DEBUG
    void print_details() const override;

    FloatType expected_average_utility_E_U_r(FloatType U_r, const BusinessConnection* business_connection) const;
    FloatType grad_expected_average_utility_E_U_r(FloatType U_r, const BusinessConnection* business_connection) const;
    FloatType estimate_marginal_utility(const BusinessConnection* bc, FloatType production_quantity_X, FloatType unit_production_costs_n_c) const;
    // CES utility specific funtions TODO: check if replacing by abstract funtions suitable
    FloatType CES_utility_function(std::vector<FloatType> consumption_demands) const;
    FloatType CES_marginal_utility(int index_of_good, double consumption_demands) const;
    FloatType CES_average_utility(int index_of_good,
                                  std::vector<FloatType> current_consumption,
                                  std::vector<FloatType> share_factors,
                                  FloatType substitution_coefficient);

    // functions for constrained optimization
    FloatType inequality_constraint(const double* x, double* grad);
    FloatType max_objective(const double* x, double* grad) const;
    void print_distribution(const std::vector<double>& demand_requests_D) const;
};
}  // namespace acclimate

#endif
