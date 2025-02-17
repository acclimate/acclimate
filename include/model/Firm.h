// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

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
    Flow baseline_production_ = Flow(0.0); /** X^* */
    Flow baseline_use_ = Flow(0.0);        /** U^* */
    Flow production_ = Flow(0.0);          /** X */
    std::shared_ptr<BusinessConnection> self_supply_connection_;

  public:
    non_owning_ptr<Sector> sector;
    const std::unique_ptr<CapacityManager> capacity_manager;
    const std::unique_ptr<SalesManager> sales_manager;

  private:
    void produce();

  public:
    Firm(id_t id, Sector* sector_, Region* region, const Ratio& possible_overcapacity_ratio_beta);
    void initialize() override;
    void iterate_consumption_and_production() override;
    void iterate_expectation() override;
    void iterate_purchase() override;
    void iterate_investment() override;
    void add_baseline_production(const Flow& flow);
    void subtract_baseline_production(const Flow& flow);
    void add_baseline_use(const Flow& flow);
    void subtract_baseline_use(const Flow& flow);
    Firm* as_firm() override { return this; }
    const Firm* as_firm() const override { return this; }
    const BusinessConnection* self_supply_connection() const;
    void self_supply_connection(std::shared_ptr<BusinessConnection> self_supply_connection_p);
    const Flow& production() const;
    const Flow& baseline_production() const { return baseline_production_; }
    Flow forced_baseline_production() const { return round(baseline_production_ * forcing_); }
    Flow maximal_production() const; /** \beta * X^* */
    FlowQuantity forced_baseline_production_quantity() const { return round(baseline_production_.get_quantity() * forcing_); }
    FloatType forced_baseline_production_quantity_float() const { return to_float(baseline_production_.get_quantity() * forcing_); }
    FlowQuantity forced_maximal_production_quantity() const;
    const Flow& baseline_use() const { return baseline_use_; }

    void debug_print_details() const override;

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return EconomicAgent::observe<Observer, H>(o)  //
               && o.set(H::hash("baseline_production"),
                        [this]() {  //
                            return baseline_production();
                        })
               && o.set(H::hash("communicated_possible_production"),
                        [this]() {  //
                            return sales_manager->communicated_parameters().possible_production;
                        })
               && o.set(H::hash("desired_production_capacity"),
                        [this]() {  //
                            return capacity_manager->get_desired_production_capacity();
                        })
               && o.set(H::hash("direct_loss"),
                        [this]() {  //
                            return Flow::possibly_negative(round(baseline_production_.get_quantity() * Forcing(1.0 - forcing_)),
                                                           production_.get_quantity() > 0.0 ? production_.get_price() : Price(0.0));
                        })
               && o.set(H::hash("expected_offer_price"),
                        [this]() {  //
                            return sales_manager->communicated_parameters().offer_price;
                        })
               && o.set(H::hash("expected_production"),
                        [this]() {  //
                            return sales_manager->communicated_parameters().expected_production;
                        })
               && o.set(H::hash("forcing"),
                        [this]() {  //
                            return forcing();
                        })
               && o.set(H::hash("incoming_demand"),
                        [this]() {  //
                            return sales_manager->sum_demand_requests();
                        })
               && o.set(H::hash("offer_price"),
                        [this]() {  //
                            return sales_manager->communicated_parameters().production.get_price();
                        })
               && o.set(H::hash("production"),
                        [this]() {  //
                            return production();
                        })
               && o.set(H::hash("production_capacity"),
                        [this]() {  //
                            return capacity_manager->get_production_capacity();
                        })
               && o.set(H::hash("possible_production_capacity"),
                        [this]() {  //
                            return capacity_manager->get_possible_production_capacity();
                        })
               && o.set(H::hash("tax"),
                        [this]() {  //
                            return sales_manager->get_tax();
                        })
               && o.set(H::hash("total_loss"),
                        [this]() {  //
                            return Flow::possibly_negative(round(baseline_production_.get_quantity() - production_.get_quantity()),
                                                           production_.get_quantity() > 0.0 ? production_.get_price() : Price(0.0));
                        })
               && o.set(H::hash("total_production_costs"),
                        [this]() {  //
                            return sales_manager->total_production_costs();
                        })
               && o.set(H::hash("total_revenue"),
                        [this]() {  //
                            return sales_manager->total_revenue();
                        })
               && o.set(H::hash("total_value_loss"),
                        [this]() {  //
                            return (baseline_production_ - production_).get_value();
                        })
               && o.set(H::hash("unit_production_costs"),
                        [this]() {  //
                            return sales_manager->communicated_parameters().possible_production.get_price();
                        })
            //
            ;
    }
};
}  // namespace acclimate

#endif
