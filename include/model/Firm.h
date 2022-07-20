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

#ifndef ACCLIMATE_FIRM_H
#define ACCLIMATE_FIRM_H

#include <memory>

#include "acclimate.h"
#include "model/CapacityManager.h"
#include "model/EconomicAgent.h"
#include "model/SalesManager.h"

namespace acclimate {

class BusinessConnection;
class Region;
class Sector;

class Firm final : public EconomicAgent {
  private:
    Flow initial_production_X_star_ = Flow(0.0);
    Flow production_X_ = Flow(0.0);  // quantity of production and its selling value
    Flow initial_total_use_U_star_ = Flow(0.0);
    std::shared_ptr<BusinessConnection> self_supply_connection_;

    Parameters::FirmParameters parameters_m;

  public:
    non_owning_ptr<Sector> sector;
    const std::unique_ptr<CapacityManager> capacity_manager;
    const std::unique_ptr<SalesManager> sales_manager;

  private:
    void produce_X();

  public:
    Firm(id_t id_p,
         Sector* sector_p,
         Region* region_p,
         const Ratio& possible_overcapacity_ratio_beta_p,
         const Ratio& upper_storage_limit_omega_p,
         const Time& initial_storage_fill_factor_psi_p);
    void initialize() override;
    void iterate_consumption_and_production() override;
    void iterate_expectation() override;
    void iterate_purchase() override;
    void iterate_investment() override;
    void add_initial_production_X_star(const Flow& initial_production_flow_X_star);
    void subtract_initial_production_X_star(const Flow& initial_production_flow_X_star);
    void add_initial_total_use_U_star(const Flow& initial_use_flow_U_star);
    void subtract_initial_total_use_U_star(const Flow& initial_use_flow_U_star);
    Firm* as_firm() override { return this; }
    const Firm* as_firm() const override { return this; }
    const BusinessConnection* self_supply_connection() const;
    void self_supply_connection(std::shared_ptr<BusinessConnection> self_supply_connection_p);
    const Flow& production_X() const;
    const Flow& initial_production_X_star() const { return initial_production_X_star_; }
    Flow forced_initial_production_lambda_X_star() const { return round(initial_production_X_star_ * forcing_m); }
    Flow maximal_production_beta_X_star() const;
    FlowQuantity forced_initial_production_quantity_lambda_X_star() const { return round(initial_production_X_star_.get_quantity() * forcing_m); }
    FloatType forced_initial_production_quantity_lambda_X_star_float() const { return to_float(initial_production_X_star_.get_quantity() * forcing_m); }
    FlowQuantity forced_maximal_production_quantity_lambda_beta_X_star() const;
    const Flow& initial_total_use_U_star() const { return initial_total_use_U_star_; }

    const Parameters::FirmParameters& parameters() const { return parameters_m; }
    Parameters::FirmParameters& parameters_writable();

    void debug_print_details() const override;

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return EconomicAgent::observe<Observer, H>(o)  //
               && o.set(H::hash("communicated_possible_production"),
                        [this]() {  //
                            return sales_manager->communicated_parameters().possible_production_X_hat;
                        })
               && o.set(H::hash("desired_production_capacity"),
                        [this]() {  //
                            return capacity_manager->get_desired_production_capacity_p_tilde();
                        })
               && o.set(H::hash("direct_loss"),
                        [this]() {  //
                            return Flow::possibly_negative(round(initial_production_X_star_.get_quantity() * Forcing(1.0 - forcing_m)),
                                                           production_X_.get_quantity() > 0.0 ? production_X_.get_price() : Price(0.0));
                        })
               && o.set(H::hash("expected_offer_price"),
                        [this]() {  //
                            return sales_manager->communicated_parameters().offer_price_n_bar;
                        })
               && o.set(H::hash("expected_production"),
                        [this]() {  //
                            return sales_manager->communicated_parameters().expected_production_X;
                        })
               && o.set(H::hash("forcing"),
                        [this]() {  //
                            return forcing();
                        })
               && o.set(H::hash("incoming_demand"),
                        [this]() {  //
                            return sales_manager->sum_demand_requests_D();
                        })
               && o.set(H::hash("offer_price"),
                        [this]() {  //
                            return sales_manager->communicated_parameters().production_X.get_price();
                        })
               && o.set(H::hash("production"),
                        [this]() {  //
                            return production_X();
                        })
               && o.set(H::hash("production_capacity"),
                        [this]() {  //
                            return capacity_manager->get_production_capacity_p();
                        })
               && o.set(H::hash("possible_production_capacity"),
                        [this]() {  //
                            return capacity_manager->get_possible_production_capacity_p_hat();
                        })
               && o.set(H::hash("tax"),
                        [this]() {  //
                            return sales_manager->get_tax();
                        })
               && o.set(H::hash("total_loss"),
                        [this]() {  //
                            return Flow::possibly_negative(round(initial_production_X_star_.get_quantity() - production_X_.get_quantity()),
                                                           production_X_.get_quantity() > 0.0 ? production_X_.get_price() : Price(0.0));
                        })
               && o.set(H::hash("total_production_costs"),
                        [this]() {  //
                            return sales_manager->total_production_costs_C();
                        })
               && o.set(H::hash("total_revenue"),
                        [this]() {  //
                            return sales_manager->total_revenue_R();
                        })
               && o.set(H::hash("total_value_loss"),
                        [this]() {  //
                            return (initial_production_X_star_ - production_X_).get_value();
                        })
               && o.set(H::hash("unit_production_costs"),
                        [this]() {  //
                            return sales_manager->communicated_parameters().possible_production_X_hat.get_price();
                        })
            //
            ;
    }
};
}  // namespace acclimate

#endif
