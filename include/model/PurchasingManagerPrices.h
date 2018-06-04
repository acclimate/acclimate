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

#ifndef ACCLIMATE_PURCHASINGMANAGERPRICES_H
#define ACCLIMATE_PURCHASINGMANAGERPRICES_H

#include "model/PurchasingManager.h"

namespace acclimate {

template<class ModelVariant>
class PurchasingManagerPrices;

template<class ModelVariant>
struct OptimizerData {
    const PurchasingManagerPrices<ModelVariant>* purchasing_manager;
    std::vector<BusinessConnection<ModelVariant>*> business_connections;
    std::vector<FloatType> upper_bounds;
    std::vector<FloatType> lower_bounds;
    FloatType last_upper_bound;
    FlowQuantity transport_flow_deficit = FlowQuantity(0.0);
};

template<class ModelVariant>
class PurchasingManagerPrices : public PurchasingManager<ModelVariant> {
  public:
    using PurchasingManager<ModelVariant>::business_connections;
    using PurchasingManager<ModelVariant>::storage;
    using PurchasingManager<ModelVariant>::get_flow_deficit;
    using PurchasingManager<ModelVariant>::id;

  protected:
    using PurchasingManager<ModelVariant>::demand_D_;
    FloatType optimized_value_ = 0.0;
    Demand purchase_ = Demand(0.0);
    FlowQuantity desired_purchase_ = FlowQuantity(0.0);
    FlowValue expected_costs_ = FlowValue(0.0);
    FlowValue total_transport_penalty_ = FlowValue(0.0);

  public:
    inline const FloatType& optimized_value() const {
        assertstepnot(PURCHASE);
        return optimized_value_;
    }
    inline const Demand storage_demand() const {
        assertstepnot(PURCHASE);
        return demand_D_ - purchase_;
    }
    inline const Demand& purchase() const {
        assertstepnot(PURCHASE);
        return purchase_;
    }
    inline const FlowValue& expected_costs(const EconomicAgent<ModelVariant>* const caller = nullptr) const {
#ifdef DEBUG
        if (caller != storage->economic_agent) {
            assertstepnot(PURCHASE);
        }
#else
        UNUSED(caller);
#endif
        return expected_costs_;
    }
    inline const FlowValue& total_transport_penalty() const {
        assertstepnot(PURCHASE);
        return total_transport_penalty_;
    }
    explicit PurchasingManagerPrices(Storage<ModelVariant>* storage_p);
    virtual ~PurchasingManagerPrices(){};
    void calc_optimization_parameters(std::vector<FloatType>& demand_requests_D_p,
                                      std::vector<BusinessConnection<ModelVariant>*>& zero_connections_p,
                                      OptimizerData<ModelVariant>& data_p) const;
    virtual void iterate_purchase() override;
    void add_initial_demand_D_star(const Demand& demand_D_p) override;
    void subtract_initial_demand_D_star(const Demand& demand_D_p) override;

  protected:
    FloatType purchase_constraint(const FloatType x[], FloatType grad[], const OptimizerData<ModelVariant>* data) const;
    FloatType objective_costs(const FloatType x[], FloatType grad[], const OptimizerData<ModelVariant>* data) const;
    inline FloatType scaled_D_r(const FloatType D_r, const BusinessConnection<ModelVariant>* bc) const;
    inline FloatType unscaled_D_r(const FloatType x, const BusinessConnection<ModelVariant>* bc) const;
    inline FloatType partial_D_r_scaled_D_r(const BusinessConnection<ModelVariant>* bc) const;
    inline FloatType scaled_objective(const FloatType obj) const;
    inline FloatType unscaled_objective(const FloatType x) const;
    inline FloatType partial_objective_scaled_objective() const;
    inline FloatType scaled_use(const FloatType use) const;
    inline FloatType unscaled_use(const FloatType x) const;
    inline FloatType partial_use_scaled_use() const;
    void calc_desired_purchase(const OptimizerData<ModelVariant>* data);
    FloatType n_r(const FloatType& D_r, const BusinessConnection<ModelVariant>* business_connection) const;
    FloatType estimate_production_extension_penalty(const BusinessConnection<ModelVariant>* bc, const FloatType production_quantity_X) const;
    FloatType estimate_marginal_production_costs(const BusinessConnection<ModelVariant>* bc,
                                                 const FloatType production_quantity_X,
                                                 const FloatType unit_production_costs_n_c) const;
    FloatType estimate_marginal_production_extension_penalty(const BusinessConnection<ModelVariant>* bc, const FloatType production_quantity_X) const;
    FloatType expected_average_price_E_n_r(const FloatType& D_r, const BusinessConnection<ModelVariant>* business_connection) const;
    FloatType transport_penalty(const FloatType& D_r, const BusinessConnection<ModelVariant>* business_connection) const;
    FloatType calc_n_co(const FloatType& n_bar_min, const FloatType& D_r_min, const BusinessConnection<ModelVariant>* business_connection) const;
    FloatType grad_n_r(const FloatType& D_r, const BusinessConnection<ModelVariant>* business_connection) const;
    FloatType grad_expected_average_price_E_n_r(const FloatType& D_r, const BusinessConnection<ModelVariant>* business_connection) const;
    FloatType partial_D_r_transport_penalty(const FloatType& D_r, const BusinessConnection<ModelVariant>* business_connection) const;
    const FlowQuantity calc_analytical_approximation_X_max(const BusinessConnection<ModelVariant>* bc) const;
    void optimize_purchase(std::vector<FloatType>& demand_requests_D_p, OptimizerData<ModelVariant>& data_p);
    void check_maximum_price_reached(std::vector<FloatType>& demand_requests_D_p, OptimizerData<ModelVariant>& data_p);
    const FlowValue check_D_in_bounds(FlowQuantity& D_r_, OptimizerData<ModelVariant>& data_p, unsigned int r) const;

#ifdef DEBUG
    void print_distribution(const FloatType demand_requests_D_p[], const OptimizerData<ModelVariant>* data, bool connection_details) const;
#endif
};
}  // namespace acclimate

#endif
