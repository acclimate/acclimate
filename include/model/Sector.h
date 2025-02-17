// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_SECTOR_H
#define ACCLIMATE_SECTOR_H

#include <string>

#include "acclimate.h"
#include "openmp.h"

namespace acclimate {

class Model;
class Firm;

class Sector final {
  public:
    enum class transport_type_t { AVIATION, IMMEDIATE, ROADSEA };

    struct Parameters {
        // keep sorted alphabetically/in groups!

        Price baseline_markup = Price(0.0);

        Price estimated_price_increase_production_extension = Price(0.0);
        Price price_increase_production_extension = Price(0.0);

        Ratio supply_elasticity = Ratio(0.0);
        Time target_storage_refill_time = Time(0.0);
        Time target_storage_withdraw_time = Time(0.0);
        Time transport_investment_adjustment_time = Time(0.0);

        template<typename Func>
        void initialize(Func&& f, Time delta_t) {
            baseline_markup = f("baseline_markup").template as<Price>();

            price_increase_production_extension = f("price_increase_production_extension").template as<Price>();
            estimated_price_increase_production_extension =
                f("estimated_price_increase_production_extension").template as<Price>(to_float(price_increase_production_extension));

            supply_elasticity = f("supply_elasticity").template as<Ratio>();
            target_storage_refill_time = f("target_storage_refill_time").template as<FloatType>() * delta_t;
            target_storage_withdraw_time = f("target_storage_withdraw_time").template as<FloatType>() * delta_t;
            transport_investment_adjustment_time = f("transport_investment_adjustment_time").template as<FloatType>() * delta_t;
        }
    };

  private:
    Demand total_demand_ = Demand(0.0);
    openmp::Lock total_demand_lock_;
    Flow total_production_ = Flow(0.0);
    openmp::Lock total_production_lock_;
    Flow last_total_production_ = Flow(0.0);
    non_owning_ptr<Model> model_;
    Parameters parameters_;

  public:
    const id_t id;
    const Ratio upper_storage_limit;         /** \omega */
    const Time baseline_storage_fill_factor; /** \psi */
    const transport_type_t transport_type;
    non_owning_vector<Firm> firms;

  public:
    Sector(Model* model, id_t id_, Ratio upper_storage_limit_, Time baseline_storage_fill_factor_, transport_type_t transport_type_);
    static transport_type_t map_transport_type(const hashed_string& transport_type);
    static const char* unmap_transport_type(transport_type_t transport_type);
    const Demand& total_demand() const;
    const Demand& total_production() const;
    const Parameters& parameters() const { return parameters_; }
    void add_demand_request(const Demand& demand_request);
    void add_production(const Flow& flow);
    void add_baseline_production(const Flow& flow);
    void subtract_baseline_production(const Flow& flow);
    void iterate_consumption_and_production();

    Model* model() { return model_; }
    const Model* model() const { return model_; }
    const std::string& name() const { return id.name; }

    template<typename Func>
    void initialize_parameters(Func&& f, Time delta_t) {
        debug::assertstep(this, IterationStep::INITIALIZATION);
        parameters_.initialize(std::move(f), delta_t);
    }

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return true  //
               && o.set(H::hash("offer_price"),
                        [this]() {  //
                            return total_production().get_price();
                        })
               && o.set(H::hash("total_production"),
                        [this]() {  //
                            return total_production();
                        })
               && o.set(H::hash("total_demand"),
                        [this]() {  //
                            return total_demand();
                        })
            //
            ;
    }
};
}  // namespace acclimate

#endif
