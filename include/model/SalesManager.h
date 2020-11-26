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

#ifndef ACCLIMATE_SALESMANAGER_H
#define ACCLIMATE_SALESMANAGER_H

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "acclimate.h"
#include "openmp.h"

namespace acclimate {

class BusinessConnection;
class Firm;
class Model;

struct SupplyParameters {
    Price offer_price_n_bar = Price(0.0);
    Flow production_X = Flow(0.0);
    Flow expected_production_X = Flow(0.0);
    Flow possible_production_X_hat = Flow(0.0);  // price = unit_production_costs_n_c
};

class SalesManager final {
  private:
    Demand sum_demand_requests_D_ = Demand(0.0);
    openmp::Lock sum_demand_requests_D_lock;
    // For communicating certain quantities to (potential) buyers
    SupplyParameters communicated_parameters_;
    Price initial_unit_commodity_costs = Price(0.0);
    FlowValue total_production_costs_C_ = FlowValue(0.0);
    FlowValue total_revenue_R_ = FlowValue(0.0);
    Flow estimated_possible_production_X_hat_ = Flow(0.0);
    Ratio tax_ = Ratio(0.0);
    struct {
        std::vector<std::shared_ptr<BusinessConnection>>::iterator connection_not_served_completely;
        Price price_cheapest_buyer_accepted_in_optimization = Price(0.0);  // Price of cheapest connection that has been considered in the profit optimization
        Flow flow_not_served_completely = Flow(0.0);
    } supply_distribution_scenario;  // to distribute production among demand requests

  public:
    non_owning_ptr<Firm> firm;
    std::vector<std::shared_ptr<BusinessConnection>> business_connections;

  private:
    std::tuple<Flow, Price> calc_supply_distribution_scenario(const Flow& possible_production_X_hat_p);
    std::tuple<Flow, Price> calc_expected_supply_distribution_scenario(const Flow& possible_production_X_hat_p);
    FlowValue calc_total_production_costs(const Flow& production_X, const Price& unit_production_costs_n_c) const;
    FlowQuantity analytic_solution_in_production_extension(const Price& unit_production_costs_n_c,
                                                           const Price& price_demand_request_not_served_completely) const;
    FlowValue calc_additional_revenue_expectation(const FlowQuantity& production_quantity_X_p, const Price& n_min_p) const;
    Price calc_marginal_revenue_curve(const FlowQuantity& production_quantity_X_p, const Price& n_min_p) const;
    Price goal_fkt_marginal_costs_minus_marginal_revenue(const FlowQuantity& production_quantity_X_p,
                                                         const Price& unit_production_costs_n_c,
                                                         const Price& n_min_p) const;
    Price goal_fkt_marginal_costs_minus_price(const FlowQuantity& production_quantity_X_p, const Price& unit_production_costs_n_c, const Price& price) const;
    Flow search_root_bisec_expectation(const FlowQuantity& left,
                                       const FlowQuantity& right,
                                       const FlowQuantity& production_quantity_X_p,
                                       const Price& unit_production_costs_n_c,
                                       const Price& n_min_p,
                                       const Price& precision_p) const;
    void print_parameters() const;
    void print_connections(std::vector<std::shared_ptr<BusinessConnection>>::const_iterator begin_equally_distributed,
                           std::vector<std::shared_ptr<BusinessConnection>>::const_iterator end_equally_distributed) const;

  public:
    explicit SalesManager(Firm* firm_p);
    ~SalesManager();
    const Demand& sum_demand_requests_D() const;
    void add_demand_request_D(const Demand& demand_request_D);
    void add_initial_demand_request_D_star(const Demand& initial_demand_request_D_star);
    void subtract_initial_demand_request_D_star(const Demand& initial_demand_request_D_star);
    bool remove_business_connection(BusinessConnection* business_connection);
    const SupplyParameters& communicated_parameters() const;
    const FlowValue& total_production_costs_C() const;
    const FlowValue& total_revenue_R() const;
    void impose_tax(const Ratio tax_p);
    FlowValue get_tax() const;
    void distribute();
    void initialize();
    void iterate_expectation();
    Flow get_transport_flow() const;
    Price get_initial_markup() const;
    Price get_initial_unit_variable_production_costs() const;
    Flow calc_production_X();
    FlowValue calc_production_extension_penalty_P(const FlowQuantity& production_quantity_X) const;
    Price calc_marginal_production_extension_penalty(const FlowQuantity& production_quantity_X) const;
    Price calc_marginal_production_costs(const FlowQuantity& production_quantity_X, const Price& unit_production_costs_n_c) const;

    void debug_print_details() const;

    Model* model();
    const Model* model() const;
    std::string name() const;
};
}  // namespace acclimate

#endif
