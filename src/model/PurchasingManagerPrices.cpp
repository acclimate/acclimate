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

#include "model/PurchasingManagerPrices.h"
#include <fstream>
#include "model/BusinessConnection.h"
#include "model/Consumer.h"
#include "model/Model.h"
#include "model/Storage.h"
#include "optimization.h"
#include "variants/ModelVariants.h"

namespace acclimate {

#ifdef OPTIMIZATION_WARNINGS
#define MAX_GRADIENT 1e3
#endif

template<class ModelVariant>
PurchasingManagerPrices<ModelVariant>::PurchasingManagerPrices(Storage<ModelVariant>* storage_p) : PurchasingManager<ModelVariant>(storage_p) {}

template<class ModelVariant>
FloatType PurchasingManagerPrices<ModelVariant>::estimate_production_extension_penalty(const BusinessConnection<ModelVariant>* bc,
                                                                                       const FloatType production_quantity_X) const {
    assert(production_quantity_X >= 0.0);
    if (production_quantity_X <= bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float()) {  // not in production extension
        return 0.0;
    }
    // in production extension
    return std::max(0.0, to_float(bc->seller->firm->sector->parameters().estimated_price_increase_production_extension)
                             / (2.0 * bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float())
                             * (production_quantity_X - bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float())
                             * (production_quantity_X - bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float()));
}

template<class ModelVariant>
FloatType PurchasingManagerPrices<ModelVariant>::estimate_marginal_production_costs(const BusinessConnection<ModelVariant>* bc,
                                                                                    const FloatType production_quantity_X,
                                                                                    const FloatType unit_production_costs_n_c) const {
    assert(production_quantity_X >= 0.0);
    assert(!std::isnan(unit_production_costs_n_c));
    if (production_quantity_X <= bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float()) {  // not in production extension
        return unit_production_costs_n_c;
    }
    // in production extension
    return unit_production_costs_n_c + estimate_marginal_production_extension_penalty(bc, production_quantity_X);
}

template<class ModelVariant>
FloatType PurchasingManagerPrices<ModelVariant>::estimate_marginal_production_extension_penalty(const BusinessConnection<ModelVariant>* bc,
                                                                                                const FloatType production_quantity_X) const {
    assert(production_quantity_X >= 0.0);
    if (production_quantity_X <= bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float()) {  // not in production extension
        return 0.0;
    }
    // in production extension
    return to_float(bc->seller->firm->sector->parameters().estimated_price_increase_production_extension)
           / bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float()
           * (production_quantity_X - bc->seller->firm->forced_initial_production_quantity_lambda_X_star_float());
}

template<class ModelVariant>
inline FloatType PurchasingManagerPrices<ModelVariant>::scaled_D_r(const FloatType D_r, const BusinessConnection<ModelVariant>* bc) const {
    return D_r / partial_D_r_scaled_D_r(bc);
}

template<class ModelVariant>
inline FloatType PurchasingManagerPrices<ModelVariant>::unscaled_D_r(const FloatType x, const BusinessConnection<ModelVariant>* bc) const {
    return x * partial_D_r_scaled_D_r(bc);
}

template<class ModelVariant>
inline FloatType PurchasingManagerPrices<ModelVariant>::partial_D_r_scaled_D_r(const BusinessConnection<ModelVariant>* bc) const {
    return to_float(bc->initial_flow_Z_star().get_quantity());
}

template<class ModelVariant>
inline FloatType PurchasingManagerPrices<ModelVariant>::scaled_objective(const FloatType obj) const {
    return obj / partial_objective_scaled_objective();
}

template<class ModelVariant>
inline FloatType PurchasingManagerPrices<ModelVariant>::unscaled_objective(const FloatType x) const {
    return x * partial_objective_scaled_objective();
}

template<class ModelVariant>
inline FloatType PurchasingManagerPrices<ModelVariant>::partial_objective_scaled_objective() const {
    return to_float(storage->initial_used_flow_U_star().get_quantity());
}

template<class ModelVariant>
inline FloatType PurchasingManagerPrices<ModelVariant>::scaled_use(const FloatType use) const {
    return use / partial_use_scaled_use();
}

template<class ModelVariant>
inline FloatType PurchasingManagerPrices<ModelVariant>::unscaled_use(const FloatType x) const {
    return x * partial_use_scaled_use();
}

template<class ModelVariant>
inline FloatType PurchasingManagerPrices<ModelVariant>::partial_use_scaled_use() const {
    return to_float(storage->initial_used_flow_U_star().get_quantity());
}

template<class ModelVariant>
void PurchasingManagerPrices<ModelVariant>::calc_desired_purchase(const OptimizerData<ModelVariant>* data) {
    Quantity S_shortage = data->transport_flow_deficit * storage->sector->model->delta_t()
                          + (storage->initial_content_S_star().get_quantity() - storage->content_S().get_quantity());
    FlowQuantity maximal_possible_purchase = FlowQuantity(0.0);
    for (std::size_t r = 0; r < data->business_connections.size(); r++) {
        FlowQuantity D_r_max = FlowQuantity(unscaled_D_r(data->upper_bounds[r], data->business_connections[r]));
        assert(!isnan(D_r_max));
        maximal_possible_purchase += D_r_max;
    }
    FlowQuantity desired_use = storage->desired_used_flow_U_tilde().get_quantity();  // desired used flow is either last or expected flow

    if (S_shortage > 0.0) {  // storage level low
        desired_purchase_ = std::min(std::max(desired_use + S_shortage / storage->sector->parameters().target_storage_refill_time, FlowQuantity(0.0)),
                                     maximal_possible_purchase);
    } else {  // shorage level high
        desired_purchase_ = std::min(std::max(desired_use + S_shortage / storage->sector->parameters().target_storage_withdraw_time, FlowQuantity(0.0)),
                                     maximal_possible_purchase);
    }
}

template<class ModelVariant>
FloatType PurchasingManagerPrices<ModelVariant>::purchase_constraint(const FloatType x[], FloatType grad[], const OptimizerData<ModelVariant>* data) const {
#ifdef DEBUG
    try {
#endif
        // has to be in the form g(D_r) <= 0
        FloatType use = 0.0;
        for (unsigned int r = 0; r < data->business_connections.size(); r++) {
            FloatType D_r = unscaled_D_r(x[r], data->business_connections[r]);
            assert(!std::isnan(D_r));
            use += D_r;
        }
        if (grad != nullptr) {
            for (unsigned int r = 0; r < data->business_connections.size(); r++) {
                grad[r] = -partial_D_r_scaled_D_r(data->business_connections[r]) * 1.0 / partial_use_scaled_use();
#ifdef OPTIMIZATION_WARNINGS
                if (grad[r] > MAX_GRADIENT) {
                    warning(std::string(*data->business_connections[r]) << ": large gradient of " << grad[r]);
                }
#endif
            }
        }
        return scaled_use(to_float(desired_purchase_) - use);
#ifdef DEBUG
    } catch (std::exception& e) {
        errinfo_(e.what() << " (in optimization)");
        throw;
    }
#endif
}

template<class ModelVariant>
FloatType PurchasingManagerPrices<ModelVariant>::objective_costs(const FloatType x[], FloatType grad[], const OptimizerData<ModelVariant>* data) const {
#ifdef DEBUG
    try {
#endif
        FloatType costs = 0.0;
        FloatType purchase = 0.0;
        for (std::size_t r = 0; r < data->business_connections.size(); r++) {
            FloatType D_r = unscaled_D_r(x[r], data->business_connections[r]);
            assert(!std::isnan(D_r));
            costs += n_r(D_r, data->business_connections[r]) * D_r + transport_penalty(D_r, data->business_connections[r]);
            purchase += D_r;
        }
        FloatType partial_correction = 0;
        if (storage->sector->model->parameters().cost_correction) {
            FloatType correction = purchase - to_float(storage->initial_used_flow_U_star().get_quantity());
            if (correction < 0) {
                partial_correction = to_float(storage->sector->model->parameters().transport_penalty_small);
            } else {
                partial_correction = to_float(storage->sector->model->parameters().transport_penalty_large);
            }
            costs -= std::abs(partial_correction * correction);
        }
        if (grad != nullptr) {
            for (std::size_t r = 0; r < data->business_connections.size(); r++) {
                FloatType D_r = unscaled_D_r(x[r], data->business_connections[r]);
                grad[r] = -partial_D_r_scaled_D_r(data->business_connections[r])
                          * (grad_n_r(D_r, data->business_connections[r]) * D_r + n_r(D_r, data->business_connections[r]) - partial_correction
                             + partial_D_r_transport_penalty(D_r, data->business_connections[r]))
                          / partial_objective_scaled_objective();
#ifdef OPTIMIZATION_WARNINGS
                if (grad[r] > MAX_GRADIENT) {
                    warning(std::string(*data->business_connections[r]) << ": large gradient of " << grad[r]);
                }
#endif
            }
        }
        return scaled_objective(-costs);
#ifdef DEBUG
    } catch (std::exception& e) {
        errinfo_(e.what() << " (in optimization)");
        throw;
    }
#endif
}

template<class ModelVariant>
FloatType PurchasingManagerPrices<ModelVariant>::expected_average_price_E_n_r(const FloatType& D_r,
                                                                              const BusinessConnection<ModelVariant>* business_connection) const {
    FloatType X = to_float(business_connection->seller->communicated_parameters().production_X.get_quantity());  // note: wo expectation X = X_expected;
    FloatType X_expected =
        to_float(business_connection->seller->communicated_parameters().expected_production_X.get_quantity());  // note: wo expectation X = X_expected;;
    FloatType Z_last = to_float(business_connection->last_shipment_Z().get_quantity());
    assert(Z_last <= X);
    Ratio ratio_X_expected_to_X = (X > 0.0) ? X_expected / X : 0.0;
    FloatType X_new = D_r + ratio_X_expected_to_X * (X - Z_last);
    assert(X_new >= 0.0);
    assert(round(FlowQuantity(X_new)) <= business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity());
    if (X_new > to_float(business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity())) {
#ifdef OPTIMIZATION_WARNINGS
        warning("X_new > X_hat: X_new = " << FlowQuantity(X_new)
                                          << " X_hat = " << business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity());
#endif
        X_new = to_float(business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity());
    }

    FloatType new_price_demand_request = 0.0;
    if (X_expected <= 0.0 && X_new <= 0.0) {  // no production in last timestep
        new_price_demand_request = to_float(business_connection->seller->communicated_parameters().offer_price_n_bar);
    } else if (X_expected <= 0.0 && X_new > 0.0) {
        new_price_demand_request = to_float(business_connection->seller->communicated_parameters().offer_price_n_bar)
                                   + estimate_production_extension_penalty(business_connection, X_new) / X_new;
    } else if (X_expected > 0.0 && X_new <= 0.0) {
        assert(estimate_production_extension_penalty(business_connection, X_expected) / X_expected
               < to_float(business_connection->seller->communicated_parameters().offer_price_n_bar));
        new_price_demand_request = to_float(business_connection->seller->communicated_parameters().offer_price_n_bar)
                                   - estimate_production_extension_penalty(business_connection, X_expected) / X_expected;
        assert(new_price_demand_request > 0.0);
    } else {  // regular case
        new_price_demand_request = to_float(business_connection->seller->communicated_parameters().offer_price_n_bar)
                                   - estimate_production_extension_penalty(business_connection, X_expected) / X_expected
                                   + estimate_production_extension_penalty(business_connection, X_new) / X_new;
    }
    assert(!std::isnan(new_price_demand_request));
    assert(new_price_demand_request > 0.0);
    return new_price_demand_request;
}

template<class ModelVariant>
FloatType PurchasingManagerPrices<ModelVariant>::n_r(const FloatType& D_r, const BusinessConnection<ModelVariant>* business_connection) const {
    assert(D_r >= 0.0);
    FloatType n_bar = to_float(business_connection->seller->communicated_parameters().offer_price_n_bar);
    FloatType Z_last = to_float(business_connection->last_shipment_Z().get_quantity());
    FloatType X = 0.0;
    FloatType X_expected = 0.0;
    X = to_float(business_connection->seller->communicated_parameters().production_X.get_quantity());
    X_expected = to_float(business_connection->seller->communicated_parameters().expected_production_X.get_quantity());

    FloatType E_n_r = expected_average_price_E_n_r(D_r, business_connection);  // expected average price for D_r
    Ratio ratio_X_expected_to_X = (X > 0.0) ? X_expected / X : 0.0;

    FloatType D_r_min =
        std::max(0.0, business_connection->seller->firm->forced_initial_production_quantity_lambda_X_star_float() - ratio_X_expected_to_X * (X - Z_last));
    FloatType X_new_min = D_r_min + ratio_X_expected_to_X * (X - Z_last);
    assert(X_new_min > 0.0);
    FloatType npe_at_X_expected = (X_expected > 0.0) ? estimate_production_extension_penalty(business_connection, X_expected) / X_expected : 0.0;
    FloatType npe_at_X_new_min = (X_new_min > 0.0) ? estimate_production_extension_penalty(business_connection, X_new_min) / X_new_min : 0.0;
    FloatType n_bar_min = n_bar - npe_at_X_expected + npe_at_X_new_min;
    FloatType n_co = calc_n_co(n_bar_min, D_r_min, business_connection);
    if (n_co <= n_bar_min) {  // in linear regime of E_n(D) curve
        if (D_r < D_r_min) {  // note: D_r_min == 0 cannot occur
            assert(D_r_min > 0.0);
            return n_co + (n_bar_min - n_co) / D_r_min * D_r;
        }
        // in production extension of E_n(D) curve
        return E_n_r;
    }
    // E_n(D) curved cropped from below with n_co
    if (E_n_r <= n_co) {
        return n_co;
    }
    return E_n_r;
}

template<class ModelVariant>
FloatType PurchasingManagerPrices<ModelVariant>::grad_expected_average_price_E_n_r(const FloatType& D_r,
                                                                                   const BusinessConnection<ModelVariant>* business_connection) const {
    FloatType X = to_float(business_connection->seller->communicated_parameters().production_X.get_quantity());  // note: wo expectation X = X_expected;
    FloatType X_expected =
        to_float(business_connection->seller->communicated_parameters().expected_production_X.get_quantity());  // note: wo expectation X = X_expected;;
    FloatType Z_last = to_float(business_connection->last_shipment_Z().get_quantity());
    Ratio ratio_X_expected_to_X = (X > 0.0) ? X_expected / X : 0.0;
    FloatType X_new = D_r + ratio_X_expected_to_X * (X - Z_last);
    assert(round(FlowQuantity(X_new)) <= business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity());
    if (X_new > to_float(business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity())) {
#ifdef OPTIMIZATION_WARNINGS
        warning("X_new > X_hat: X_new = " << FlowQuantity(X_new)
                                          << " X_hat = " << business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity());
#endif
        X_new = to_float(business_connection->seller->communicated_parameters().possible_production_X_hat.get_quantity());
    }
    if (X_new <= 0.0) {
        return 0.0;
    }
    // regular case
    return estimate_marginal_production_extension_penalty(business_connection, X_new) / X_new
           - estimate_production_extension_penalty(business_connection, X_new) / X_new / X_new;
}

template<class ModelVariant>
FloatType PurchasingManagerPrices<ModelVariant>::calc_n_co(const FloatType& n_bar_min,
                                                           const FloatType& D_r_min,
                                                           const BusinessConnection<ModelVariant>* business_connection) const {
    FloatType n_co =
        estimate_marginal_production_costs(business_connection, to_float(business_connection->seller->communicated_parameters().production_X.get_quantity()),
                                           business_connection->seller->communicated_parameters().possible_production_X_hat.get_price_float());
    if (storage->sector->model->parameters().maximal_decrease_reservation_price_limited_by_markup) {
        FloatType n_crit = n_bar_min - to_float(storage->sector->parameters().initial_markup) * D_r_min;
        n_co = std::max(n_co, n_crit);
    }
    return n_co;
}

template<class ModelVariant>
FloatType PurchasingManagerPrices<ModelVariant>::grad_n_r(const FloatType& D_r, const BusinessConnection<ModelVariant>* business_connection) const {
    FloatType n_bar = to_float(business_connection->seller->communicated_parameters().offer_price_n_bar);
    FloatType X =
        to_float(business_connection->seller->communicated_parameters().expected_production_X.get_quantity());  // note: wo expectation X = X_expected;
    FloatType X_expected =
        to_float(business_connection->seller->communicated_parameters().expected_production_X.get_quantity());  // note: wo expectation X = X_expected;;
    FloatType Z_last = to_float(business_connection->last_shipment_Z().get_quantity());
    Ratio ratio_X_expected_to_X = (X > 0.0) ? X_expected / X : 0.0;
    FloatType grad_E_n_r = grad_expected_average_price_E_n_r(D_r, business_connection);  // expected gradient of average price for D_r
    FloatType E_n_r = expected_average_price_E_n_r(D_r, business_connection);            // expected average price for D_r
    FloatType D_r_min = std::max(0.0,
                                 business_connection->seller->firm->forced_initial_production_quantity_lambda_X_star_float()
                                     - ratio_X_expected_to_X * (X - Z_last));  // note: D_r_star is the demand request for X_new=lambda * X_star
    assert(D_r_min >= 0.0);
    FloatType X_new_min = D_r_min + ratio_X_expected_to_X * (X - Z_last);
    assert(X_new_min > 0.0);
    FloatType npe_at_X_expected = (X_expected > 0.0) ? estimate_production_extension_penalty(business_connection, X_expected) / X_expected : 0.0;
    FloatType npe_at_X_new_min = (X_new_min > 0.0) ? estimate_production_extension_penalty(business_connection, X_new_min) / X_new_min : 0.0;
    FloatType n_bar_min = n_bar - npe_at_X_expected + npe_at_X_new_min;
    FloatType n_co = calc_n_co(n_bar_min, D_r_min, business_connection);
    if (n_co <= n_bar_min) {  // in linear regime of E_n(D) curve
        if (D_r < D_r_min && D_r_min > 0.0) {
            return (n_bar_min - n_co) / D_r_min;
        }
        // in production extension of E_n(D) curve
        return grad_E_n_r;
    }
    // E_n(D) curved cropped from below with n_co
    if (E_n_r <= n_co) {
        return 0.0;
    }
    return grad_E_n_r;
}

template<class ModelVariant>
FloatType PurchasingManagerPrices<ModelVariant>::transport_penalty(const FloatType& D_r, const BusinessConnection<ModelVariant>* business_connection) const {
    FloatType target = 0.0;
    if (storage->sector->model->parameters().deviation_penalty) {
        target = to_float(business_connection->last_demand_request_D(this).get_quantity());
    } else {
        target = to_float(business_connection->initial_flow_Z_star().get_quantity());
    }
    if (storage->sector->model->parameters().quadratic_transport_penalty) {
        FloatType marg_penalty = 0.0;
        if (D_r < target) {
            marg_penalty = -to_float(storage->sector->parameters().initial_markup);
        } else if (D_r > target) {
            marg_penalty = to_float(storage->sector->parameters().initial_markup);
        } else {
            marg_penalty = 0.0;
        }
        if (storage->sector->model->parameters().relative_transport_penalty) {
            if (target > FlowQuantity::precision) {
                return (D_r - target)
                       * ((D_r - target) * to_float(storage->sector->model->parameters().transport_penalty_large) / (target * target) / 2.0 + marg_penalty);
            }
            return D_r * D_r * to_float(storage->sector->model->parameters().transport_penalty_large / 2.0 + Price(marg_penalty));
        }
        return (D_r - target) * ((D_r - target) * to_float(storage->sector->model->parameters().transport_penalty_large) / 2.0 + marg_penalty);
    }
    if (storage->sector->model->parameters().relative_transport_penalty) {
        return partial_D_r_transport_penalty(D_r, business_connection) * (D_r - target) / target;
    }
    return partial_D_r_transport_penalty(D_r, business_connection) * (D_r - target);
}

template<class ModelVariant>
FloatType PurchasingManagerPrices<ModelVariant>::partial_D_r_transport_penalty(const FloatType& D_r,
                                                                               const BusinessConnection<ModelVariant>* business_connection) const {
    FloatType target = 0.0;
    if (storage->sector->model->parameters().deviation_penalty) {
        target = to_float(business_connection->last_demand_request_D(this).get_quantity());
    } else {
        target = to_float(business_connection->initial_flow_Z_star().get_quantity());
    }
    if (storage->sector->model->parameters().quadratic_transport_penalty) {
        FloatType marg_penalty = 0.0;
        if (D_r < target) {
            marg_penalty = -to_float(storage->sector->parameters().initial_markup);
        } else if (D_r > target) {
            marg_penalty = to_float(storage->sector->parameters().initial_markup);
        } else {
            marg_penalty = 0.0;
        }
        if (storage->sector->model->parameters().relative_transport_penalty) {
            if (target > FlowQuantity::precision) {
                return (D_r - target) * to_float(storage->sector->model->parameters().transport_penalty_large) / (target * target) + marg_penalty;
            }
            return D_r * to_float(storage->sector->model->parameters().transport_penalty_large) + marg_penalty;
        }
        return (D_r - target) * to_float(storage->sector->model->parameters().transport_penalty_large) + marg_penalty;
    }
    if (storage->sector->model->parameters().relative_transport_penalty) {
        if (D_r < target) {
            return -to_float(storage->sector->model->parameters().transport_penalty_small) / target;
        }
        if (D_r > target) {
            return to_float(storage->sector->model->parameters().transport_penalty_large) / target;
        }
        return (to_float(storage->sector->model->parameters().transport_penalty_large) - to_float(storage->sector->model->parameters().transport_penalty_small))
               / 2.0 / target;
    }
    if (D_r < target) {
        return -to_float(storage->sector->model->parameters().transport_penalty_small);
    }
    if (D_r > target) {
        return to_float(storage->sector->model->parameters().transport_penalty_large);
    }
    return (to_float(storage->sector->model->parameters().transport_penalty_large) - to_float(storage->sector->model->parameters().transport_penalty_small))
           / 2.0;
}

template<class ModelVariant>
void PurchasingManagerPrices<ModelVariant>::add_initial_demand_D_star(const Demand& demand_D_p) {
    assertstep(INITIALIZATION);
    demand_D_ += demand_D_p;
    expected_costs_ += demand_D_p.get_value();
}

template<class ModelVariant>
void PurchasingManagerPrices<ModelVariant>::subtract_initial_demand_D_star(const Demand& demand_D_p) {
    assertstep(INITIALIZATION);
    demand_D_ -= demand_D_p;
    expected_costs_ -= demand_D_p.get_value();
}

template<class ModelVariant>
void PurchasingManagerPrices<ModelVariant>::iterate_purchase() {
    assertstep(PURCHASE);
    assert(!business_connections.empty());
    std::vector<FloatType> demand_requests_D;  // demand requests considered in optimization
    OptimizerData<ModelVariant> data;
    std::vector<BusinessConnection<ModelVariant>*> zero_connections;
    // sets upper_limits, pushes_back corresponding upper bound and sets the corresponding initial value demand_requests_D, and adds upper limit to
    // max_possible_demand_quantity with profit optimization: demand is not needed anymore for calculation of E_i, and can be used to determine upper bound for
    // sum_r D_r
    calc_optimization_parameters(demand_requests_D, zero_connections, data);

    optimized_value_ = 0.0;
    purchase_ = Demand(0.0);
    desired_purchase_ = FlowQuantity(0.0);
    expected_costs_ = FlowValue(0.0);
    total_transport_penalty_ = FlowValue(0.0);

    calc_desired_purchase(&data);
    if (round(desired_purchase_) <= 0.0) {
        for (auto& zero_connection : business_connections) {
            zero_connection->send_demand_request_D(Demand(0.0));
        }
        return;
    }

    demand_D_ = Demand(0.0);
    if (!data.business_connections.empty()) {
        optimize_purchase(demand_requests_D, data);

        FloatType costs = 0.0;
        FloatType use = 0.0;
        // distribute demand requests
        for (std::size_t r = 0; r < data.business_connections.size(); r++) {
            FloatType D_r;
            D_r = unscaled_D_r(demand_requests_D[r], data.business_connections[r]);
            Demand demand_request_D = Demand(FlowQuantity(D_r), FlowValue(D_r));
            assert(!std::isnan(n_r(D_r, data.business_connections[r])));
            demand_request_D.set_price(round(Price(n_r(D_r, data.business_connections[r]))));

#ifdef OPTIMIZATION_WARNINGS
            if (round(demand_request_D.get_quantity()) > round(FlowQuantity(unscaled_D_r(data.upper_bounds[r], data.business_connections[r])))) {
                warning(std::string(*data.business_connections[r])
                        << ": upper limit overshot in optimization D_r: " << demand_request_D.get_quantity() << " D_max: " << data.upper_bounds[r]
                        << " D_star: " << data.business_connections[r]->initial_flow_Z_star().get_quantity());
            }
            if (round(demand_request_D.get_quantity()) < round(FlowQuantity(unscaled_D_r(data.lower_bounds[r], data.business_connections[r])))) {
                warning(std::string(*data.business_connections[r])
                        << ": lower limit overshot in optimization D_r: " << demand_request_D.get_quantity() << " D_min: " << data.lower_bounds[r]
                        << " D_star: " << data.business_connections[r]->initial_flow_Z_star().get_quantity());
            }
#endif
            data.business_connections[r]->send_demand_request_D(round(demand_request_D));

            use += D_r;
            demand_D_ += round(demand_request_D);
            costs += n_r(D_r, data.business_connections[r]) * D_r + transport_penalty(D_r, data.business_connections[r]);
            total_transport_penalty_ += FlowValue(transport_penalty(D_r, data.business_connections[r]));
        }
        expected_costs_ = FlowValue(costs);
        assert(FlowQuantity(use) >= 0.0);
        purchase_ = demand_D_;

    } else {
        warning("possible demand is zero (no supplier with possible production capacity > 0.0)");
    }
    // push zero demand request to the purchasers we do consult in this timestep
    for (auto& zero_connection : zero_connections) {
        zero_connection->send_demand_request_D(Demand(0.0));
    }
}

static const char* get_optimization_results(const int result_p) {
    switch (result_p) {
        case 1:
            return "Generic success";
        case 2:
            return "Optimization stopped because stopval was reached";
        case 3:
            return "Optimization stopped because ftol_rel or ftol_abs was reached";
        case 4:
            return "Optimization stopped because xtol_rel or xtol_abs was reached";
        case 5:
            return "Optimization stopped because maxeval was reached";
        case 6:
            return "Optimization stopped because maxtime was reached";
        case -1:
            return "Generic failure code";
        case -2:
            return "Invalid arguments(e.g. lower bounds are bigger than upper bounds, an unknown algorithm was specified, etc.";
        case -3:
            return "Ran out of memory";
        case -4:
            return "Halted because roundoff errors limited progress.(In this case, the optimization still typically returns a useful result.) ";
        case -5:
            return "Halted because of a forced termination: the user called nlopt_force_stop(opt) on the optimization’s nlopt_opt object opt from the user’s "
                   "objective function or constraints.";
        default:
            return "Unknown optimization result";
    }
}

template<class ModelVariant>
void PurchasingManagerPrices<ModelVariant>::optimize_purchase(std::vector<FloatType>& demand_requests_D_p, OptimizerData<ModelVariant>& data_p) {
    if (data_p.business_connections.empty()) {
        return;
    }
    const unsigned int dimensions_optimization_problem = data_p.business_connections.size();
    nlopt::result result = nlopt::SUCCESS;
    try {
        nlopt::opt opt(static_cast<nlopt::algorithm>(storage->sector->model->parameters().optimization_algorithm), dimensions_optimization_problem);

        std::vector<FloatType> xtol_abs(data_p.business_connections.size());
        for (unsigned int i = 0; i < data_p.business_connections.size(); i++) {
            xtol_abs[i] = scaled_D_r(FlowQuantity::precision, data_p.business_connections[i]);
        }

        opt.add_equality_constraint(
            [](unsigned n, const double* x, double* grad, void* data) {
                UNUSED(n);
                const OptimizerData<ModelVariant>* d = static_cast<OptimizerData<ModelVariant>*>(data);
                return d->purchasing_manager->purchase_constraint(x, grad, d);
            },
            &data_p, FlowQuantity::precision);

        opt.set_max_objective(
            [](unsigned n, const double* x, double* grad, void* data) {
                UNUSED(n);
                const OptimizerData<ModelVariant>* d = static_cast<OptimizerData<ModelVariant>*>(data);
                return d->purchasing_manager->objective_costs(x, grad, d);
            },
            &data_p);

        opt.set_xtol_abs(xtol_abs);
        opt.set_lower_bounds(data_p.lower_bounds);
        opt.set_upper_bounds(data_p.upper_bounds);
        opt.set_maxeval(storage->sector->model->parameters().optimization_maxiter);
        opt.set_maxtime(storage->sector->model->parameters().optimization_timeout);  // timeout given in sec
        result = opt.optimize(demand_requests_D_p, optimized_value_);                // note: optimized_value_is value of objective function
        optimized_value_ = unscaled_objective(optimized_value_);

    } catch (const nlopt::roundoff_limited& exc) {
#define IGNORE_ROUNDOFFLIMITED
#ifndef IGNORE_ROUNDOFFLIMITED
        ONLY_DEBUG(print_distribution(&demand_requests_D_p[0], &data_p, false));
        Acclimate::Run<ModelVariant>::instance()->event(EventType::OPTIMIZER_ROUNDOFF_LIMITED, storage->sector, nullptr, storage->economic_agent);
#ifdef OPTIMIZATION_PROBLEMS_FATAL
        error("optimization is roundoff limited (for " << data_p.business_connections.size() << " inputs)");
#else
        warning("optimization is roundoff limited (for " << data_p.business_connections.size() << " inputs)");
#endif
#endif
#if !(defined(DEBUG) && defined(OPTIMIZATION_PROBLEMS_FATAL))
    } catch (const std::exception& exc) {
        ONLY_DEBUG(print_distribution(&demand_requests_D_p[0], &data_p, false));
        Acclimate::Run<ModelVariant>::instance()->event(EventType::OPTIMIZER_FAILURE, storage->sector, nullptr, storage->economic_agent);
#ifdef OPTIMIZATION_PROBLEMS_FATAL
        error("optimization failed, " << exc.what() << " (for " << data_p.business_connections.size() << " inputs)");
#else
        warning("optimization failed, " << exc.what() << " (for " << data_p.business_connections.size() << " inputs)");
#endif
#endif
    }
    if (result < nlopt::SUCCESS) {
        ONLY_DEBUG(print_distribution(&demand_requests_D_p[0], &data_p, false));
        Acclimate::Run<ModelVariant>::instance()->event(EventType::OPTIMIZER_FAILURE, storage->sector, nullptr, storage->economic_agent);
#ifndef OPTIMIZATION_PROBLEMS_FATAL
        if (result == nlopt::INVALID_ARGS) {
#endif
            error("optimization failed, " << get_optimization_results(result) << " (for " << data_p.business_connections.size() << " inputs)");
#ifndef OPTIMIZATION_PROBLEMS_FATAL
        } else {
            warning("optimization failed, " << get_optimization_results(result) << " (for " << data_p.business_connections.size() << " inputs)");
        }
#endif
    } else if (result == nlopt::MAXTIME_REACHED) {
        ONLY_DEBUG(print_distribution(&demand_requests_D_p[0], &data_p, false));
        Acclimate::Run<ModelVariant>::instance()->event(EventType::OPTIMIZER_TIMEOUT, storage->sector, nullptr, storage->economic_agent);
#ifdef OPTIMIZATION_PROBLEMS_FATAL
        error("optimization timed out (for " << data_p.business_connections.size() << " inputs)");
#else
        warning("optimization timed out (for " << data_p.business_connections.size() << " inputs)");
#endif
    }
    if (result != nlopt::XTOL_REACHED) {
        warning("optimization finished with " << get_optimization_results(result));
    }
}

template<class ModelVariant>
void PurchasingManagerPrices<ModelVariant>::calc_optimization_parameters(std::vector<FloatType>& demand_requests_D_p,
                                                                         std::vector<BusinessConnection<ModelVariant>*>& zero_connections_p,
                                                                         OptimizerData<ModelVariant>& data_p) const {
    if (storage->desired_used_flow_U_tilde().get_quantity() <= 0.0) {
        warning("desired_used_flow_U_tilde is zero : no purchase");
        zero_connections_p = business_connections;
        return;
    }
    demand_requests_D_p.reserve(business_connections.size());
    data_p.transport_flow_deficit = get_flow_deficit();
    data_p.purchasing_manager = this;
    data_p.business_connections.reserve(business_connections.size());
    data_p.lower_bounds.reserve(business_connections.size());
    data_p.upper_bounds.reserve(business_connections.size());

    FloatType max_possible_demand_quantity = 0.0;
    FloatType total_upper_limit = 0.0;
    FloatType total_initial_value = 0.0;
    for (const auto& bc : business_connections) {
        if (bc->seller->communicated_parameters().possible_production_X_hat.get_quantity() <= 0.0) {
            zero_connections_p.push_back(bc);
        } else {  // this supplier can deliver a non-zero amount
            // assumption, we cannot crowd out other purchasers given that our maximum offer price is n_max, calculate analytical approximation for maximal
            // deliverable amount of purchaser X_max(n_max) and consider boundary conditions
            const FloatType X = to_float(bc->seller->communicated_parameters().production_X.get_quantity());
#ifdef DEBUG
            const FloatType X_hat = to_float(bc->seller->communicated_parameters().possible_production_X_hat.get_quantity());
#endif
            const FloatType Z_last = to_float(bc->last_shipment_Z().get_quantity());
            const FloatType X_expected = to_float(bc->seller->communicated_parameters().expected_production_X.get_quantity());  // wo expectation X_expected = X
            const FloatType ratio_X_expected_to_X = (X > 0.0) ? X_expected / X : 0.0;
            const FloatType X_max = to_float(calc_analytical_approximation_X_max(bc));
            assert(X_max <= X_hat);
            const FloatType lower_limit = 0.0;
            const FloatType upper_limit = X_max - ratio_X_expected_to_X * (X - Z_last);
            // in expected_average_price_E_n_r may happen, but it could also be the case that the optimizer does not respect D_r in [D_r_min,D_r_max]
            if (upper_limit > 0.0) {
                const FloatType initial_value = std::min(upper_limit, std::max(lower_limit, ratio_X_expected_to_X * Z_last));
                data_p.business_connections.push_back(bc);
                data_p.lower_bounds.push_back(scaled_D_r(lower_limit, bc));
                data_p.upper_bounds.push_back(scaled_D_r(upper_limit, bc));
                demand_requests_D_p.push_back(scaled_D_r(initial_value, bc));
                total_upper_limit += upper_limit;
                total_initial_value += initial_value;
                max_possible_demand_quantity += upper_limit;
            } else {
                zero_connections_p.push_back(bc);
            }
        }
    }
}

template<class ModelVariant>
const FlowQuantity PurchasingManagerPrices<ModelVariant>::calc_analytical_approximation_X_max(const BusinessConnection<ModelVariant>* bc) const {
    return bc->seller->communicated_parameters().possible_production_X_hat.get_quantity();
}

#ifdef DEBUG
template<class ModelVariant>
void PurchasingManagerPrices<ModelVariant>::print_distribution(const FloatType demand_requests_D_p[],
                                                               const OptimizerData<ModelVariant>* data,
                                                               bool connection_details) const {
#pragma omp critical(output)
    {
        std::vector<FloatType> demand_requests_D(data->business_connections.size());
        std::copy(demand_requests_D_p, demand_requests_D_p + data->business_connections.size(), std::begin(demand_requests_D));
        std::cout << Acclimate::instance()->timeinfo() << ", " << std::string(*this) << ": demand distribution for " << data->business_connections.size()
                  << " inputs :\n";
        FloatType purchasing_quantity = 0.0;
        FloatType purchasing_value = 0.0;
        FloatType initial_sum = 0.0;
        std::vector<FloatType> last_demand_requests(data->business_connections.size());
        std::vector<FloatType> grad(data->business_connections.size());

        FloatType obj = 0;
        obj = objective_costs(&demand_requests_D[0], &grad[0], data);
        FloatType total_upper_bound = 0.0;
        FloatType T_penalty = 0.0;
        for (std::size_t r = 0; r < data->business_connections.size(); r++) {
            FloatType D_r = unscaled_D_r(demand_requests_D[r], data->business_connections[r]);
            FloatType lower_bound_D_r = unscaled_D_r(data->lower_bounds[r], data->business_connections[r]);
            FloatType upper_bound_D_r = unscaled_D_r(data->upper_bounds[r], data->business_connections[r]);
            FloatType X = to_float(data->business_connections[r]->seller->communicated_parameters().production_X.get_quantity());
            FloatType X_hat = to_float(data->business_connections[r]->seller->communicated_parameters().possible_production_X_hat.get_quantity());
            FloatType X_star = to_float(data->business_connections[r]->seller->firm->initial_production_X_star().get_quantity());

            total_upper_bound += X_hat;
            FloatType n_bar = to_float(data->business_connections[r]->seller->communicated_parameters().offer_price_n_bar);
            FloatType n_r_l = n_r(D_r, data->business_connections[r]);
            FloatType n_r_tc_l = transport_penalty(D_r, data->business_connections[r]);
            T_penalty += n_r_tc_l;
            last_demand_requests[r] = to_float(data->business_connections[r]->last_demand_request_D(this).get_quantity());
#ifdef OPTIMIZATION_PROBLEMS_FATAL
            UNUSED(connection_details);
            {
#else
            if (connection_details) {
#endif
                std::cout << "      " << std::string(*data->business_connections[r]) << " :\n"
                          << PRINT_ROW1("X", FlowQuantity(X)) << PRINT_ROW1("X_star", FlowQuantity(X_star)) << PRINT_ROW1("X_hat", FlowQuantity(X_hat))
                          << PRINT_ROW1("n_bar", FlowQuantity(n_bar)) << PRINT_ROW1("n_r(D_r)", FlowQuantity(n_r_l))
                          << PRINT_ROW2("penalty_t", FlowQuantity(n_r_tc_l), scaled_objective(n_r_tc_l))
                          << PRINT_ROW2("Z_last", data->business_connections[r]->last_shipment_Z().get_quantity(),
                                        scaled_D_r(to_float(data->business_connections[r]->last_shipment_Z().get_quantity()), data->business_connections[r]))
                          << PRINT_ROW1("D_star", data->business_connections[r]->initial_flow_Z_star().get_quantity())
                          << PRINT_ROW1("out_share", data->business_connections[r]->initial_flow_Z_star()
                                                         / data->business_connections[r]->seller->firm->initial_production_X_star())
                          << PRINT_ROW1("grad", grad[r])
                          << PRINT_ROW2("D_lower", FlowQuantity(lower_bound_D_r), scaled_D_r(lower_bound_D_r, data->business_connections[r]))
                          << PRINT_ROW2("D_r", FlowQuantity(D_r), scaled_D_r(D_r, data->business_connections[r]))
                          << PRINT_ROW2("D_upper", FlowQuantity(upper_bound_D_r), scaled_D_r(upper_bound_D_r, data->business_connections[r])) << '\n';
            }
            purchasing_quantity += D_r;
            purchasing_value += n_r_l * D_r;
            initial_sum += to_float(data->business_connections[r]->initial_flow_Z_star().get_quantity());
        }
        FloatType use = purchasing_quantity;
        FloatType S_shortage =
            to_float(data->transport_flow_deficit)
            + to_float((storage->initial_content_S_star().get_quantity() - storage->content_S().get_quantity()) / storage->sector->model->delta_t());
        FloatType n_r_bar = (purchasing_quantity > 0.0) ? purchasing_value / purchasing_quantity : 0.0;
        std::cout << "      Storage :\n"
                  << PRINT_ROW1("av. n_r", Price(n_r_bar)) << PRINT_ROW1("S", storage->content_S().get_quantity())
                  << PRINT_ROW1("S_star", storage->initial_content_S_star().get_quantity()) << PRINT_ROW1("S_shortage", Quantity(S_shortage))
                  << PRINT_ROW2("T_def", data->transport_flow_deficit, scaled_use(to_float(data->transport_flow_deficit)))
                  << PRINT_ROW2("T_penalty", FlowValue(T_penalty), scaled_objective(T_penalty)) << "\n";

        std::cout << "      Sums :\n"
                  << PRINT_ROW1("X_hat", FlowQuantity(total_upper_bound)) << PRINT_ROW1("D_orig", demand_D_.get_quantity())
                  << PRINT_ROW2("purchase", FlowQuantity(purchasing_quantity), scaled_use(use)) << PRINT_ROW1("D_star", FlowQuantity(initial_sum))
                  << PRINT_ROW2("U_i", FlowQuantity(use), scaled_use(use)) << PRINT_ROW2("obj", FlowValue(unscaled_objective(obj)), obj) << "\n";
    }
}
#endif

INSTANTIATE_PRICES(PurchasingManagerPrices);
}  // namespace acclimate
