// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_STORAGE_H
#define ACCLIMATE_STORAGE_H

#include <memory>
#include <string>
#include <vector>

#include "acclimate.h"
#include "model/PurchasingManager.h"
#include "openmp.h"

namespace acclimate {

class EconomicAgent;
class Model;
class Sector;

class Storage final {
  public:
    struct Parameters {
        Ratio consumption_price_elasticity = Ratio(0.0);

        template<typename Func>
        void initialize(Func&& f) {
            consumption_price_elasticity = f("consumption_price_elasticity").template as<Ratio>();
        }
    };

  private:
    Flow input_flow_[3] = {Flow(0.0), Flow(0.0), Flow(0.0)}; /** I */
    Forcing forcing_ = Forcing(1.0);                         /** \mu */
    Stock content_ = Stock(0.0);                             /** S */
    Stock baseline_content_ = Stock(0.0);                    /** S^* */
    Flow baseline_input_flow_ = Flow(0.0);                   /** I^* = U^* */
    Flow used_flow_ = Flow(0.0);                             /** U */
    Flow desired_used_flow_ = Flow(0.0);                     /** \tilde{U} */
    openmp::Lock input_flow_lock_;
    Parameters parameters_;

  public:
    non_owning_ptr<Sector> sector;
    non_owning_ptr<EconomicAgent> economic_agent;
    const std::unique_ptr<PurchasingManager> purchasing_manager;
    const id_t id;

  private:
    void calc_content();

  public:
    Storage(Sector* sector_, EconomicAgent* economic_agent_);
    ~Storage();
    const Stock& content() const;
    const Flow& used_flow(const EconomicAgent* caller = nullptr) const;
    const Flow& desired_used_flow(const EconomicAgent* caller = nullptr) const;
    const Stock& baseline_content() const { return baseline_content_; }
    const Flow& baseline_input_flow() const { return baseline_input_flow_; }
    const Flow& baseline_used_flow() const { return baseline_input_flow_; }  // baseline_used_flow == baseline_input_flow_
    const Parameters& parameters() const { return parameters_; }
    void set_desired_used_flow(const Flow& desired_used_flow_p);
    void use_content(const Flow& flow);
    Flow last_possible_use() const;
    Flow estimate_possible_use() const;
    Flow get_possible_use() const;
    void push_flow(const Flow& flow);
    const Flow& current_input_flow() const;
    const Flow& last_input_flow() const;
    const Flow& next_input_flow() const;
    Ratio get_technology_coefficient() const;
    Ratio get_input_share() const;
    void add_baseline_flow(const Flow& flow);
    bool subtract_baseline_flow(const Flow& flow);
    void iterate_consumption_and_production();

    Model* model();
    const Model* model() const;
    const std::string& name() const { return id.name; }

    template<typename Func>
    void initialize_parameters(Func&& f) {
        debug::assertstep(this, IterationStep::INITIALIZATION);
        parameters_.initialize(std::move(f));
    }

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return true  //
               && o.set(H::hash("business_connections"),
                        [this]() {  //
                            return purchasing_manager->business_connections.size();
                        })
               && o.set(H::hash("content"),
                        [this]() {  //
                            return content();
                        })
               && o.set(H::hash("demand"),
                        [this]() {  //
                            return purchasing_manager->demand();
                        })
               && o.set(H::hash("desired_used_flow"),
                        [this]() {  //
                            return desired_used_flow();
                        })
               && o.set(H::hash("expected_costs"),
                        [this]() {  //
                            return purchasing_manager->expected_costs();
                        })
               && o.set(H::hash("input_flow"),
                        [this]() {  //
                            return last_input_flow();
                        })
               && o.set(H::hash("optimized_value"),
                        [this]() {  //
                            return purchasing_manager->optimized_value();
                        })
               && o.set(H::hash("possible_use"),
                        [this]() {  //
                            return last_possible_use();
                        })
               && o.set(H::hash("purchase"),
                        [this]() {  //
                            return purchasing_manager->purchase();
                        })
               && o.set(H::hash("shipment"),
                        [this]() {  //
                            return purchasing_manager->get_sum_of_last_shipments();
                        })
               && o.set(H::hash("storage_demand"),
                        [this]() {  //
                            return purchasing_manager->storage_demand();
                        })
               && o.set(H::hash("total_transport_penalty"),
                        [this]() {  //
                            return purchasing_manager->total_transport_penalty();
                        })
               && o.set(H::hash("use"),
                        [this]() {  //
                            return purchasing_manager->demand();
                        })
               && o.set(H::hash("used_flow"),
                        [this]() {  //
                            return used_flow();
                        })
            //
            ;
    }
};
}  // namespace acclimate

#endif
