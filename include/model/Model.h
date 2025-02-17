// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_MODEL_H
#define ACCLIMATE_MODEL_H

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "ModelRun.h"
#include "acclimate.h"
#include "optimization.h"

namespace acclimate {

class EconomicAgent;
class GeoLocation;
class PurchasingManager;
class Region;
class Sector;

class Model final {
    friend class ModelRun;

  public:
    struct Parameters {
        // keep sorted alphabetically/in groups!

        bool always_extend_expected_demand_curve;  // extend incoming demand in expectation step with certain elasticity
        bool budget_inequality_constrained;
        bool cheapest_price_range_generic_size;
        bool cheapest_price_range_preserve_seller_price;
        Price cheapest_price_range_width;
        std::vector<std::string> debug_purchasing_steps;  // purchasing steps where details should be printed to output, e.g. "WHOT->third_income_quintile:BFA"
                                                          // TODO move to run parameters
        bool deviation_penalty;
        bool elastic_budget;
        int lagrangian_algorithm;
        bool maximal_decrease_reservation_price_limited_by_markup;
        Ratio min_storage;
        bool naive_expectations;  // incoming demand is never extended
        bool purchasing_halfway_baseline;
        bool quadratic_transport_penalty;  // quadratic instead of linear transport penalty
        bool relative_transport_penalty;
        bool respect_markup_in_production_extension;
        bool start_purchasing_at_baseline;
        Price transport_penalty_large;
        Price transport_penalty_small;
        bool with_investment_dynamics;

        // parameters for local optimization in purchase
        bool local_purchasing_optimization;
        int optimization_algorithm;
        int optimization_maxiter;               // maximal iteration
        int optimization_precision_adjustment;  // factor for precision of global algorithms
        bool optimization_restart_baseline;
        unsigned int optimization_timeout;  // timeout in sec

        // parameters for global optimization in purchase
        bool global_purchasing_optimization;
        int global_optimization_algorithm;
        int global_optimization_maxiter;               // maximal iteration of global algorithms
        int global_optimization_precision_adjustment;  // factor for precision of global algorithms
        unsigned int global_optimization_timeout;      // timeout in sec

        // parameters for local optimization in consumption
        int utility_optimization_algorithm;
        int utility_optimization_maxiter;               // maximal iteration for utility
        int utility_optimization_precision_adjustment;  // factor for precision of utility algorithms
        unsigned int utility_optimization_timeout;      // timeout in sec

        // parameters for global optimization in consumption
        bool global_utility_optimization;
        int global_utility_optimization_algorithm;
        int global_utility_optimization_maxiter;               // maximal iteration of global utility algorithms
        int global_utility_optimization_precision_adjustment;  // factor for precision of global utility algorithms
        unsigned int global_utility_optimization_random_points;
        unsigned int global_utility_optimization_timeout;  // timeout in sec

        template<typename Func>
        void initialize(Func&& f) {
            always_extend_expected_demand_curve = f("always_extend_expected_demand_curve").template as<bool>(false);
            budget_inequality_constrained = f("budget_inequality_constrained").template as<bool>(false);
            cheapest_price_range_generic_size = (f("cheapest_price_range_width").template as<std::string>() == "auto");
            cheapest_price_range_preserve_seller_price = f("cheapest_price_range_preserve_seller_price").template as<bool>(false);
            if (!cheapest_price_range_generic_size) {
                cheapest_price_range_width = f("cheapest_price_range_width").template as<Price>();
            }
            debug_purchasing_steps = f("debug_purchasing_steps").template to_vector<std::string>();
            deviation_penalty = f("deviation_penalty").template as<bool>(false);
            elastic_budget = f("elastic_budget").template as<bool>(false);
            lagrangian_algorithm = optimization::get_algorithm(f("lagrangian_optimization_algorithm").template as<hashed_string>("augmented_lagrangian"));
            maximal_decrease_reservation_price_limited_by_markup = f("maximal_decrease_reservation_price_limited_by_markup").template as<bool>(false);
            min_storage = f("min_storage").template as<Ratio>(0.0);
            naive_expectations = f("naive_expectations").template as<bool>(true);
            purchasing_halfway_baseline = f("purchasing_halfway_baseline").template as<bool>(false);
            quadratic_transport_penalty = f("quadratic_transport_penalty").template as<bool>(true);
            relative_transport_penalty = f("relative_transport_penalty").template as<bool>(true);
            start_purchasing_at_baseline = f("start_purchasing_at_baseline").template as<bool>(false);
            transport_penalty_large = f("transport_penalty_large").template as<Price>();
            transport_penalty_small = f("transport_penalty_small").template as<Price>();
            with_investment_dynamics = f("with_investment_dynamics").template as<bool>(false);

            // parameters for local optimization in purchase
            local_purchasing_optimization = f("local_purchasing_optimization").template as<bool>(true);
            optimization_algorithm = optimization::get_algorithm(f("optimization_algorithm").template as<hashed_string>("slsqp"));
            optimization_maxiter = f("optimization_maxiter").template as<int>();
            optimization_precision_adjustment = f("optimization_precision_adjustment").template as<float>(1.0);
            optimization_restart_baseline = f("optimization_restart_baseline").template as<bool>(false);
            optimization_timeout = f("optimization_timeout").template as<unsigned int>();

            // parameters for global optimization in purchase
            global_purchasing_optimization = f("global_purchasing_optimization").template as<bool>(false);
            global_optimization_algorithm = optimization::get_algorithm(f("global_optimization_algorithm").template as<hashed_string>("crs"));
            global_optimization_maxiter = f("global_optimization_maxiter").template as<int>(optimization_maxiter);
            global_optimization_precision_adjustment = f("global_optimization_precision_adjustment").template as<float>(1.0);
            global_optimization_timeout = f("global_optimization_timeout").template as<unsigned int>(optimization_timeout);

            // parameters for local optimization in consumption
            utility_optimization_algorithm = optimization::get_algorithm(f("utility_optimization_algorithm").template as<hashed_string>("slsqp"));
            utility_optimization_maxiter = f("utility_optimization_maxiter").template as<int>(optimization_maxiter);
            utility_optimization_precision_adjustment = f("utility_optimization_precision_adjustment").template as<float>(1.0);
            utility_optimization_timeout = f("utility_optimization_timeout").template as<unsigned int>(optimization_timeout);

            // parameters for global optimization in consumption
            global_utility_optimization = f("global_utility_optimization").template as<bool>(false);
            global_utility_optimization_algorithm =
                optimization::get_algorithm(f("global_utility_optimization_algorithm").template as<hashed_string>("mlsl_low_discrepancy"));
            global_utility_optimization_maxiter = f("global_utility_optimization_maxiter").template as<int>(global_optimization_maxiter);
            global_utility_optimization_precision_adjustment = f("global_utility_optimization_precision_adjustment").template as<float>(1.0);
            global_utility_optimization_random_points = f("global_sampling_points").template as<int>(64);
            global_utility_optimization_timeout = f("global_utility_optimization_timeout").template as<unsigned int>(global_optimization_timeout);
        }
    };

  private:
    Time time_ = Time(0.0);
    TimeStep timestep_ = 0;
    Time delta_t_ = Time(1.0);
    unsigned char current_register_ = 1;
    std::vector<std::pair<PurchasingManager*, std::size_t>> parallelized_storages;
    std::vector<std::pair<EconomicAgent*, std::size_t>> parallelized_agents;
    non_owning_ptr<ModelRun> run_;
    Parameters parameters_;

  public:
    owning_vector<Sector> sectors;
    owning_vector<Region> regions;
    owning_vector<GeoLocation> other_locations;
    owning_vector<EconomicAgent> economic_agents;

  private:
    explicit Model(ModelRun* run);

  public:
    Model(const Model& other) = delete;
    Model(Model&& other) = default;
    ~Model();
    const Time& time() const { return time_; }
    const TimeStep& timestep() const { return timestep_; }
    const Time& delta_t() const { return delta_t_; }
    bool is_first_timestep() const { return timestep_ == 0; }
    void switch_registers();
    void tick();
    void set_delta_t(const Time& delta_t);
    const unsigned char& current_register() const { return current_register_; }
    unsigned char other_register() const { return 1 - current_register_; }
    const Parameters& parameters() const { return parameters_; }
    void start();
    void iterate_consumption_and_production();
    void iterate_expectation();
    void iterate_purchase();
    void iterate_investment();

    ModelRun* run() { return run_; }
    const ModelRun* run() const { return run_; }
    const Model* model() const { return this; }
    std::string name() const { return "MODEL"; }

    template<typename Func>
    void initialize_parameters(Func&& f) {
        debug::assertstep(this, IterationStep::INITIALIZATION);
        parameters_.initialize(std::move(f));
    }

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return true  //
               && o.set(H::hash("duration"),
                        [this]() {  //
                            return run()->duration();
                        })
            //
            ;
    }
};

}  // namespace acclimate

#endif
