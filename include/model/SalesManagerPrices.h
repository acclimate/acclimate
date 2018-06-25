/*
  Copyright (C) 2014-2017 Sven Willner <sven.willner@pik-potsdam.de>
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

#ifndef ACCLIMATE_SALESMANAGERPRICES_H
#define ACCLIMATE_SALESMANAGERPRICES_H

#include <memory>
#include <tuple>
#include <vector>
#include "run.h"
#include "model/SalesManager.h"
#include "types.h"

namespace acclimate {

template<class Modelvariant>
class BusinessConnection;
template<class Modelvariant>
class Firm;

struct SupplyParameters {
    Price offer_price_n_bar = Price(0.0);
    Flow production_X = Flow(0.0);
    Flow expected_production_X = Flow(0.0);
    Flow possible_production_X_hat = Flow(0.0);  // price = unit_production_costs_n_c
};

template<class ModelVariant>
class SalesManagerPrices : public SalesManager<ModelVariant> {
  public:
    using SalesManager<ModelVariant>::business_connections;
    using SalesManager<ModelVariant>::firm;
    using SalesManager<ModelVariant>::id;
    using SalesManager<ModelVariant>::model;

  protected:
    using SalesManager<ModelVariant>::sum_demand_requests_D_;

  private:
    // For communicating certain quantities to (potential) buyers
    SupplyParameters communicated_parameters_;
    Price initial_unit_commodity_costs = Price(0.0);
    FlowValue total_production_costs_C_ = FlowValue(0.0);
    FlowValue total_revenue_R_ = FlowValue(0.0);
    Flow estimated_possible_production_X_hat_ = Flow(0.0);
    Ratio tax_ = Ratio(0.0);
    struct {
        typename std::vector<std::unique_ptr<BusinessConnection<ModelVariant>>>::iterator connection_not_served_completely;
        Price price_cheapest_buyer_accepted_in_optimization = Price(0.0);  // Price of cheapest connection, that has been considered in the profit optimization
        Flow flow_not_served_completely = Flow(0.0);
    } supply_distribution_scenario;  // to distribute production among demand requests

  public:
    inline const SupplyParameters& communicated_parameters() const {
        assertstepnot(CONSUMPTION_AND_PRODUCTION);
        return communicated_parameters_;
    }
    inline const FlowValue& total_production_costs_C() const {
        assertstepnot(CONSUMPTION_AND_PRODUCTION);
        return total_production_costs_C_;
    }
    inline const FlowValue& total_revenue_R() const {
        assertstepnot(CONSUMPTION_AND_PRODUCTION);
        return total_revenue_R_;
    }
    inline void impose_tax(const Ratio tax_p) {
        assertstep(EXPECTATION);
        tax_ = tax_p;
    }
    inline const FlowValue get_tax() const { return tax_ * firm->production_X().get_value(); }

  public:
    explicit SalesManagerPrices(Firm<ModelVariant>* firm_p);
    ~SalesManagerPrices() = default;

    void distribute(const Flow& _) override;
    void initialize();
    void iterate_expectation() override;
    const Price get_initial_unit_value_added() const;
    const Price get_initial_markup() const;
    const Price get_initial_unit_variable_production_costs() const;
    const Flow calc_production_X();
    const FlowValue calc_production_extension_penalty_P(const FlowQuantity& production_quantity_X) const;
    const Price calc_marginal_production_extension_penalty(const FlowQuantity& production_quantity_X) const;
    const Price calc_marginal_production_costs(const FlowQuantity& production_quantity_X, const Price& unit_production_costs_n_c) const;

  private:
    std::tuple<Flow, Price> calc_supply_distribution_scenario(const Flow& possible_production_X_hat_p);
    std::tuple<Flow, Price> calc_expected_supply_distribution_scenario(const Flow& possible_production_X_hat_p);
    const Price calc_marginal_production_costs_plus_markup(const FlowQuantity& production_quantity_X, const Price& unit_production_costs_n_c) const;
    const FlowValue calc_total_production_costs(const Flow& production_X, const Price& unit_production_costs_n_c) const;
    const FlowQuantity analytic_solution_in_production_extension(const Price& unit_production_costs_n_c,
                                                                 const Price& price_demand_request_not_served_completely) const;
    const FlowValue calc_additional_revenue_expectation(const FlowQuantity& production_quantity_X_p, const Price& n_min_p) const;
    const Price calc_marginal_revenue_curve(const FlowQuantity& production_quantity_X_p, const Price& n_min_p) const;
    const Price goal_fkt_marginal_costs_minus_marginal_revenue(const FlowQuantity& production_quantity_X_p,
                                                               const Price& unit_production_costs_n_c,
                                                               const Price& n_min_p) const;
    const Price goal_fkt_marginal_costs_minus_price(const FlowQuantity& production_quantity_X_p,
                                                    const Price& unit_production_costs_n_c,
                                                    const Price& price) const;
    const Flow search_root_bisec_expectation(const FlowQuantity& left,
                                             const FlowQuantity& right,
                                             const FlowQuantity& production_quantity_X_p,
                                             const Price& unit_production_costs_n_c,
                                             const Price& n_min_p,
                                             const Price& precision_p) const;
#ifdef DEBUG
    void print_parameters() const;
    void print_connections(typename std::vector<std::unique_ptr<BusinessConnection<ModelVariant>>>::const_iterator begin_equally_distributed,
                           typename std::vector<std::unique_ptr<BusinessConnection<ModelVariant>>>::const_iterator end_equally_distributed) const;
#endif
};
}  // namespace acclimate

#endif
