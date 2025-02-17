// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

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
    bool utilitarian_;

    // parameters of utility function
    std::vector<std::pair<std::vector<Sector*>, FloatType>> consumer_baskets_;
    std::vector<std::vector<int>> consumer_basket_indizes_;
    FloatType inter_basket_substitution_coefficient_;
    FloatType inter_basket_substitution_exponent_;

    std::vector<FloatType> intra_basket_substitution_coefficient_;
    std::vector<FloatType> intra_basket_substitution_exponent_;

    std::vector<FloatType> basket_share_factors_;
    std::vector<FloatType> exponent_basket_share_factors_;
    std::vector<FloatType> share_factors_;
    std::vector<FloatType> exponent_share_factors_;

    // optimization parameters
    std::vector<double> optimizer_consumption_;
    // variables for (in)equality constraint, pre-allocated to increase efficiency
    FlowValue consumption_budget_;
    FlowValue not_spent_budget_;  // TODO: introduce real saving possibility, for now just trying to improve numerical stability

    // consumption limits considered in optimization
    std::vector<Price> consumption_prices_;  // prices to be considered in optimization
    std::vector<Flow> previous_consumption_;

    FloatType baseline_utility_ = 0;  // baseline utility for scaling
    std::vector<Flow> baseline_consumption_;
    // field to store utility
    FloatType utility_ = 0;
    FloatType local_optimal_utility_ = 0;

    // variables for utility function , pre-allocated to increase efficiency
    autodiff::Value<FloatType> autodiff_consumption_utility_ = autodiff::Value<FloatType>(0, 0.0);
    autodiff::Value<FloatType> autodiff_basket_consumption_utility_ = autodiff::Value<FloatType>(0, 0.0);
    autodiff::Value<FloatType> autodiffutility_ = autodiff::Value<FloatType>(0, 0.0);

    autodiff::Variable<FloatType> var_optimizer_consumption_ = autodiff::Variable<FloatType>(0, 0.0);

  public:
    using EconomicAgent::input_storages;
    using EconomicAgent::region;

  public:
    Consumer(id_t id,
             Region* region,
             FloatType inter_basket_substitution_coefficient,
             std::vector<std::pair<std::vector<Sector*>, FloatType>> consumer_baskets,
             bool utilitarian);

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
                            return utility_;
                        })
               && o.set(H::hash("local_optimal_utility"),
                        [this]() {  //
                            return local_optimal_utility_;
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

    autodiff::Value<FloatType> autodiff_nested_CES_utility_function(const autodiff::Variable<FloatType>& consumption);

    // some helpers for local comparison of old consumer and utilitarian
    std::pair<std::vector<Flow>, FloatType> utilitarian_consumption_optimization();
    void consume_optimisation_result(std::vector<Flow> consumption);

    // function for constrained optimization
    void consumption_optimize(optimization::Optimization& optimizer);

    // scaling functions
    static FlowQuantity invert_scaling_double_to_quantity(double scaling_factor, FlowQuantity scaling_quantity);
    static double invert_scaling_double_to_double(double scaled_value, FlowQuantity scaling_quantity);
    static double scale_quantity_to_double(FlowQuantity quantity, FlowQuantity scaling_quantity);
    static double scale_double_to_double(double not_scaled_double, FlowQuantity scaling_quantity);
};
}  // namespace acclimate

#endif
