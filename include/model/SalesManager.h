// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

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
    Price offer_price = Price(0.0); /** \bar{n} */
    Flow production = Flow(0.0);    /** X */
    Flow expected_production = Flow(0.0);
    Flow possible_production = Flow(0.0); /** \hat{X}, price = unit_production_costs_n_c */
};

class SalesManager final {
  private:
    Demand sum_demand_requests_ = Demand(0.0);
    openmp::Lock sum_demand_requests_lock_;
    // For communicating certain quantities to (potential) buyers
    SupplyParameters communicated_parameters_;
    Price baseline_unit_commodity_costs_ = Price(0.0);
    FlowValue total_production_costs_ = FlowValue(0.0);
    FlowValue total_revenue_ = FlowValue(0.0);
    Flow estimated_possible_production_ = Flow(0.0);
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
    std::tuple<Flow, Price> calc_supply_distribution_scenario(const Flow& possible_production);
    std::tuple<Flow, Price> calc_expected_supply_distribution_scenario(const Flow& possible_production);
    FlowValue calc_total_production_costs(const Flow& production_X, const Price& unit_production_costs_n_c) const;
    FlowQuantity analytic_solution_in_production_extension(const Price& unit_production_costs_n_c,
                                                           const Price& price_demand_request_not_served_completely) const;
    FlowValue calc_additional_revenue_expectation(const FlowQuantity& production_quantity, const Price& n_min) const;
    Price calc_marginal_revenue_curve(const FlowQuantity& production_quantity, const Price& n_min) const;
    Price goal_fkt_marginal_costs_minus_marginal_revenue(const FlowQuantity& production_quantity,
                                                         const Price& unit_production_costs_n_c,
                                                         const Price& n_min) const;
    Price goal_fkt_marginal_costs_minus_price(const FlowQuantity& production_quantity, const Price& unit_production_costs_n_c, const Price& price) const;
    Flow search_root_bisec_expectation(const FlowQuantity& left,
                                       const FlowQuantity& right,
                                       const FlowQuantity& production_quantity,
                                       const Price& unit_production_costs_n_c,
                                       const Price& n_min,
                                       const Price& precision) const;
    void print_parameters() const;
    void print_connections(std::vector<std::shared_ptr<BusinessConnection>>::const_iterator begin_equally_distributed,
                           std::vector<std::shared_ptr<BusinessConnection>>::const_iterator end_equally_distributed) const;

  public:
    explicit SalesManager(Firm* firm_);
    ~SalesManager();
    const Demand& sum_demand_requests() const;
    void add_demand_request(const Demand& demand_request);
    void add_baseline_demand(const Demand& demand);
    void subtract_baseline_demand(const Demand& demand);
    bool remove_business_connection(BusinessConnection* business_connection);
    const SupplyParameters& communicated_parameters() const;
    const FlowValue& total_production_costs() const; /** C */
    const FlowValue& total_revenue() const;          /** R */
    void impose_tax(const Ratio tax);
    FlowValue get_tax() const;
    void distribute();
    void initialize();
    void iterate_expectation();
    Flow get_transport_flow() const;
    Price get_baseline_markup() const;
    Price get_baseline_unit_variable_production_costs() const;
    Flow calc_production();
    FlowValue calc_production_extension_penalty(const FlowQuantity& production_quantity) const;
    Price calc_marginal_production_extension_penalty(const FlowQuantity& production_quantity) const;
    Price calc_marginal_production_costs(const FlowQuantity& production_quantity, const Price& unit_production_costs) const;

    void debug_print_details() const;

    Model* model();
    const Model* model() const;
    std::string name() const;
};
}  // namespace acclimate

#endif
