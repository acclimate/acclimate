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

#include <map>

#include "Sector.h"
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
    std::map<hash_t, int> input_storage_map;

    // parameters of utility function
    std::vector<std::pair<std::vector<Sector*>, FloatType>> consumer_baskets;

    FloatType inter_basket_substitution_coefficient;
    FloatType inter_basket_substitution_exponent;

    std::vector<FloatType> intra_basket_substitution_coefficient;
    std::vector<FloatType> intra_basket_substitution_exponent;

    std::vector<FloatType> basket_share_factors;
    std::vector<FloatType> share_factors;

    // optimization parameters
    std::vector<double> optimizer_consumption;
    // variables for (in)equality constraint, pre-allocated to increase efficiency
    FlowValue consumption_budget;
    FlowValue not_spent_budget;  // TODO: introduce real saving possibility, for now just trying to improve numerical stability

    // consumption limits considered in optimization
    std::vector<Price> consumption_prices;  // prices to be considered in optimization
    std::vector<Flow> previous_consumption;

    FloatType baseline_utility;  // baseline utility for scaling
    std::vector<Flow> baseline_consumption;

    // field to store utility
    FloatType utility;
    FloatType local_optimal_utility;

    // variables for utility function , pre-allocated to increase efficiency
    FloatType consumption_utility;
    FloatType basket_consumption_utility;
    autodiff::Value<FloatType> autodiff_consumption_utility = autodiff::Value<FloatType>(0, 0);
    autodiff::Value<FloatType> autodiff_basket_consumption_utility = autodiff::Value<FloatType>(0, 0);
    autodiff::Variable<FloatType> var_optimizer_consumption = autodiff::Variable<FloatType>(0, 0);
    autodiff::Value<FloatType> autodiffutility = autodiff::Value<FloatType>(0, 0);

  public:
    using EconomicAgent::input_storages;
    using EconomicAgent::region;



  public:
    Consumer(id_t id_p,
             Region* region_p,
             FloatType inter_basket_substitution_coefficient_p,
             std::vector<std::pair<std::vector<Sector*>, FloatType>> consumer_baskets_p,
             bool utilitarian_p);

    Consumer* as_consumer() override { return this; };
    const Consumer* as_consumer() const override { return this; };

    FloatType inequality_constraint(const double* x, double* grad);
    FloatType equality_constraint(const double* x, double* grad);
    FloatType max_objective(const double* x, double* grad);

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return EconomicAgent::observe<Observer, H>(o)  //
               && o.set(H::hash("utility"),
                        [this]() {  //
                            return utility;
                        })
               && o.set(H::hash("local_optimal_utility"),
                        [this]() {  //
                            return local_optimal_utility;
                        })
            //
            ;
    }

  private:
    void initialize() override;
    void iterate_consumption_and_production() override;
    void iterate_expectation() override;
    void iterate_purchase() override;
    void iterate_investment() override;

    void debug_print_details() const override;
    void debug_print_distribution();

    // CES utility specific funtions TODO: check if replacing by abstract funtions suitable
    FloatType CES_utility_function(const std::vector<FloatType>& consumption_demands);
    FloatType CES_utility_function(const std::vector<Flow>& consumption_demands);
    autodiff::Value<FloatType> autodiff_nested_CES_utility_function(const autodiff::Variable<FloatType>& consumption_demands);

    // some helpers for local comparison of old consumer and utilitarian
    std::vector<Flow> utilitarian_consumption_optimization();
    void utilitarian_consumption_execution(std::vector<Flow> desired_consumption);

    // function for constrained optimization
    void consumption_optimize(optimization::Optimization& optimizer);

    // scaling functions
    double invert_scaling_double_to_quantity(double scaling_factor, FlowQuantity scaling_quantity) const;
    double scale_quantity_to_double(FlowQuantity quantity, FlowQuantity scaling_quantity) const;
    double scale_double_to_double(double not_scaled_double, FlowQuantity scaling_quantity) const;

    // helper function to find input storage for sector:
    std::string input_storage_name(Sector* sector) { return sector->name() + "->" + this->name(); }
};
}  // namespace acclimate

#endif
