// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_PURCHASINGMANAGER_H
#define ACCLIMATE_PURCHASINGMANAGER_H

#include <memory>
#include <string>
#include <vector>

#include "acclimate.h"

namespace acclimate {

class BusinessConnection;
class EconomicAgent;
class Model;
class Storage;

namespace optimization {
class Optimization;
}

class PurchasingManager final {
    friend class optimization::Optimization;

  private:
    Demand demand_ = Demand(0.0); /** D */
    FloatType optimized_value_ = 0.0;
    Demand purchase_ = Demand(0.0);
    FlowQuantity desired_purchase_ = FlowQuantity(0.0);
    FlowValue expected_costs_ = FlowValue(0.0);
    FlowValue total_transport_penalty_ = FlowValue(0.0);
    std::vector<BusinessConnection*> purchasing_connections;
    std::vector<FloatType> demand_requests_;  // demand requests considered in optimization
    std::vector<double> upper_bounds;
    std::vector<double> lower_bounds;
    std::vector<double> xtol_abs;
    std::vector<double> pre_xtol_abs;

  public:
    non_owning_ptr<Storage> storage;
    std::vector<std::shared_ptr<BusinessConnection>> business_connections;

  private:
    FloatType run_optimizer(optimization::Optimization& opt);
    void optimization_exception_handling(bool res, optimization::Optimization& opt);
    FloatType equality_constraint(const double* x, double* grad) const;
    FloatType max_objective(const double* x, double* grad) const;
    static FloatType scaled_D_r(FloatType D_r, const BusinessConnection* bc);
    static FloatType unscaled_D_r(FloatType x, const BusinessConnection* bc);
    static FloatType partial_D_r_scaled_D_r(const BusinessConnection* bc);
    FloatType scaled_objective(FloatType obj) const;
    FloatType unscaled_objective(FloatType x) const;
    FloatType partial_objective_scaled_objective() const;
    FloatType scaled_use(FloatType use) const;
    FloatType unscaled_use(FloatType x) const;
    FloatType partial_use_scaled_use() const;
    FloatType n_r(FloatType D_r, const BusinessConnection* business_connection) const;
    static FloatType estimate_production_extension_penalty(const BusinessConnection* bc, FloatType production_quantity_X);
    static FloatType estimate_marginal_production_costs(const BusinessConnection* bc, FloatType production_quantity_X, FloatType unit_production_costs_n_c);
    static FloatType estimate_marginal_production_extension_penalty(const BusinessConnection* bc, FloatType production_quantity_X);
    FloatType expected_average_price_E_n_r(FloatType D_r, const BusinessConnection* business_connection) const;
    FloatType transport_penalty(FloatType D_r, const BusinessConnection* business_connection) const;
    FloatType calc_n_co(FloatType n_bar_min, FloatType D_r_min, const BusinessConnection* business_connection) const;
    FloatType grad_n_r(FloatType D_r, const BusinessConnection* business_connection) const;
    FloatType grad_expected_average_price_E_n_r(FloatType D_r, const BusinessConnection* business_connection) const;
    FloatType partial_D_r_transport_penalty(FloatType D_r, const BusinessConnection* business_connection) const;
    static FlowQuantity calc_analytical_approximation_X_max(const BusinessConnection* bc);
    static FloatType expected_production(const BusinessConnection* business_connection);
    static FloatType expected_additional_production(const BusinessConnection* business_connection);

    void debug_print_distribution(const std::vector<double>& demand_requests_) const;

  public:
    explicit PurchasingManager(Storage* storage_);
    ~PurchasingManager();
    const Demand& demand(const EconomicAgent* caller = nullptr) const;
    FlowQuantity get_flow_deficit() const;
    Flow get_transport_flow() const;
    Flow get_sum_of_last_shipments() const;
    void iterate_consumption_and_production() const;
    bool remove_business_connection(const BusinessConnection* business_connection);
    FloatType optimized_value() const;
    Demand storage_demand() const;
    const Demand& purchase() const;
    const FlowValue& expected_costs(const EconomicAgent* caller = nullptr) const;
    const FlowValue& total_transport_penalty() const;
    Flow get_disequilibrium() const;
    FloatType get_stddeviation() const;
    void iterate_investment();
    void iterate_purchase();
    void add_baseline_demand(const Demand& demand);
    void subtract_baseline_demand(const Demand& demand);

    void debug_print_details() const;

    const Model* model() const;
    Model* model();
    std::string name() const;

    int optimization_restart_count = 0;  // TODO: remove after debug
};
}  // namespace acclimate

#endif
