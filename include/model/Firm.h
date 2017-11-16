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

#ifndef ACCLIMATE_FIRM_H
#define ACCLIMATE_FIRM_H

#include "model/EconomicAgent.h"
#include "model/Region.h"
#include "model/Sector.h"

namespace acclimate {

template<class Modelvariant>
class BusinessConnection;

template<class ModelVariant>
class Firm : public EconomicAgent<ModelVariant> {
  private:
    Flow initial_production_X_star_ = Flow(0.0);
    Flow production_X_ = Flow(0.0);  // quantity of production and its selling value
    Flow initial_total_use_U_star_ = Flow(0.0);
    BusinessConnection<ModelVariant>* self_supply_connection_ = nullptr;

  protected:
    using EconomicAgent<ModelVariant>::forcing_;

  public:
    using EconomicAgent<ModelVariant>::input_storages;
    using EconomicAgent<ModelVariant>::region;
    using EconomicAgent<ModelVariant>::sector;
    std::unique_ptr<typename ModelVariant::CapacityManagerType> const capacity_manager;
    std::unique_ptr<typename ModelVariant::SalesManagerType> const sales_manager;

  public:
    Firm<ModelVariant>* as_firm() override;
    const Firm<ModelVariant>* as_firm() const override;
    inline const BusinessConnection<ModelVariant>* self_supply_connection() const {
        assertstepnot(CONSUMPTION_AND_PRODUCTION);
        return self_supply_connection_;
    };
    inline void self_supply_connection(BusinessConnection<ModelVariant>* self_supply_connection_p) {
        assertstep(INITIALIZATION);
        self_supply_connection_ = self_supply_connection_p;
    };
    inline const Flow& production_X() const {
        assertstepnot(CONSUMPTION_AND_PRODUCTION);
        return production_X_;
    };
    inline const Flow& initial_production_X_star() const { return initial_production_X_star_; };
    inline const Flow& initial_total_use_U_star() const { return initial_total_use_U_star_; };
    inline const Flow forced_initial_production_lambda_X_star() const { return round(initial_production_X_star_ * forcing_); };
    inline const Flow maximal_production_beta_X_star() const { return round(initial_production_X_star_ * capacity_manager->possible_overcapacity_ratio_beta); };
    inline const Flow forced_maximal_production_lambda_beta_X_star() const {
        return round(initial_production_X_star_ * forcing_ * capacity_manager->possible_overcapacity_ratio_beta);
    };
    inline const FlowQuantity& initial_production_quantity_X_star() const { return initial_production_X_star_.get_quantity(); };
    inline const FlowQuantity forced_initial_production_quantity_lambda_X_star() const {
        return round(initial_production_X_star_.get_quantity() * forcing_);
    };
    inline FloatType forced_initial_production_quantity_lambda_X_star_float() const {
        return to_float(initial_production_X_star_.get_quantity() * forcing_);
    };
    inline const FlowQuantity maximal_production_quantity_beta_X_star() const {
        return round(initial_production_X_star_.get_quantity() * capacity_manager->possible_overcapacity_ratio_beta);
    };
    inline FloatType maximal_production_quantity_beta_X_star_float() const {
        return to_float(initial_production_X_star_.get_quantity() * capacity_manager->possible_overcapacity_ratio_beta);
    };
    const FlowQuantity forced_maximal_production_quantity_lambda_beta_X_star() const {
        return round(initial_production_X_star_.get_quantity() * (capacity_manager->possible_overcapacity_ratio_beta * forcing_));
    };
    FloatType forced_maximal_production_quantity_lambda_beta_X_star_float() const {
        return to_float(initial_production_X_star_.get_quantity()) * (capacity_manager->possible_overcapacity_ratio_beta * forcing_);
    };
    inline const Flow direct_loss() const {
        return Flow(round(initial_production_X_star_.get_quantity() * Forcing(1.0 - forcing_)),
                    production_X_.get_quantity() > 0.0 ? production_X_.get_price() : Price(0.0), true);
    };
    inline const Flow total_loss() const {
        return Flow(round(initial_production_X_star_.get_quantity() - production_X_.get_quantity()),
                    production_X_.get_quantity() > 0.0 ? production_X_.get_price() : Price(0.0), true);
    };
    inline const FlowValue total_value_loss() const { return (initial_production_X_star_ - production_X_).get_value(); };

  protected:
    void produce_X();

  public:
    Firm(Sector<ModelVariant>* sector_p, Region<ModelVariant>* region_p, const Ratio& possible_overcapacity_ratio_beta_p);
    void iterate_consumption_and_production() override;
    void iterate_expectation() override;
    void iterate_purchase() override;
    void iterate_investment() override;
    void add_initial_production_X_star(const Flow& initial_production_flow_X_star);
    void subtract_initial_production_X_star(const Flow& initial_production_flow_X_star);
    void add_initial_total_use_U_star(const Flow& initial_use_flow_U_star);
    void subtract_initial_total_use_U_star(const Flow& initial_use_flow_U_star);
#ifdef DEBUG
    void print_details() const override;
#endif
};
}  // namespace acclimate

#endif
