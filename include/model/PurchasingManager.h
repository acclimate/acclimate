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

class PurchasingManager {
    friend class optimization::Optimization;

  private:
    Demand demand_D_ = Demand(0.0);
    FloatType optimized_value_ = 0.0;
    Demand purchase_ = Demand(0.0);
    FlowQuantity desired_purchase_ = FlowQuantity(0.0);
    FlowValue expected_costs_ = FlowValue(0.0);
    FlowValue total_transport_penalty_ = FlowValue(0.0);
    std::vector<BusinessConnection*> purchasing_connections;
    std::vector<FloatType> demand_requests_D;  // demand requests considered in optimization
    std::vector<double> upper_bounds;
    std::vector<double> lower_bounds;
    std::vector<double> xtol_abs;

  public:
    non_owning_ptr<Storage> storage;
    std::vector<std::shared_ptr<BusinessConnection>> business_connections;

  private:
    FloatType equality_constraint(const double* x, double* grad) const;
    FloatType max_objective(const double* x, double* grad) const;
    FloatType scaled_D_r(FloatType D_r, const BusinessConnection* bc) const;
    FloatType unscaled_D_r(FloatType x, const BusinessConnection* bc) const;
    static FloatType partial_D_r_scaled_D_r(const BusinessConnection* bc);
    FloatType scaled_objective(FloatType obj) const;
    FloatType unscaled_objective(FloatType x) const;
    FloatType partial_objective_scaled_objective() const;
    FloatType scaled_use(FloatType use) const;
    FloatType unscaled_use(FloatType x) const;
    FloatType partial_use_scaled_use() const;
    FloatType n_r(FloatType D_r, const BusinessConnection* business_connection) const;
    FloatType estimate_production_extension_penalty(const BusinessConnection* bc, FloatType production_quantity_X) const;
    FloatType estimate_marginal_production_costs(const BusinessConnection* bc, FloatType production_quantity_X, FloatType unit_production_costs_n_c) const;
    FloatType estimate_marginal_production_extension_penalty(const BusinessConnection* bc, FloatType production_quantity_X) const;
    FloatType expected_average_price_E_n_r(FloatType D_r, const BusinessConnection* business_connection) const;
    FloatType transport_penalty(FloatType D_r, const BusinessConnection* business_connection) const;
    FloatType calc_n_co(FloatType n_bar_min, FloatType D_r_min, const BusinessConnection* business_connection) const;
    FloatType grad_n_r(FloatType D_r, const BusinessConnection* business_connection) const;
    FloatType grad_expected_average_price_E_n_r(FloatType D_r, const BusinessConnection* business_connection) const;
    FloatType partial_D_r_transport_penalty(FloatType D_r, const BusinessConnection* business_connection) const;
    static FlowQuantity calc_analytical_approximation_X_max(const BusinessConnection* bc);
    // DEBUG
    void print_distribution(const std::vector<double>& demand_requests_D) const;

  public:
    explicit PurchasingManager(Storage* storage_p);
    ~PurchasingManager();
    const Demand& demand_D(const EconomicAgent* const caller = nullptr) const;
    FlowQuantity get_flow_deficit() const;
    Flow get_transport_flow() const;
    Flow get_sum_of_last_shipments() const;
    void iterate_consumption_and_production();
    bool remove_business_connection(const BusinessConnection* business_connection);
    FloatType optimized_value() const;
    Demand storage_demand() const;
    const Demand& purchase() const;
    const FlowValue& expected_costs(const EconomicAgent* const caller = nullptr) const;
    const FlowValue& total_transport_penalty() const;
    Flow get_disequilibrium() const;
    FloatType get_stddeviation() const;
    void iterate_purchase();
    void add_initial_demand_D_star(const Demand& demand_D_p);
    void subtract_initial_demand_D_star(const Demand& demand_D_p);
    Model* model() const;
    std::string id() const;
    // DEBUG
    void print_details() const;
};
}  // namespace acclimate

#endif
