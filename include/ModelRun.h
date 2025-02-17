// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_RUN_H
#define ACCLIMATE_RUN_H

#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "acclimate.h"

namespace settings {
class SettingsNode;
}

namespace acclimate {

enum class EventType : unsigned char {
    NO_CONSUMPTION,
    STORAGE_UNDERRUN,
    STORAGE_OVERRUN,
    NO_PRODUCTION_SUPPLY_SHORTAGE,
    NO_PRODUCTION_DEMAND_QUANTITY_SHORTAGE,
    NO_PRODUCTION_DEMAND_VALUE_SHORTAGE,
    NO_PRODUCTION_HIGH_COSTS,
    NO_EXP_PRODUCTION_SUPPLY_SHORTAGE,
    NO_EXP_PRODUCTION_DEMAND_QUANTITY_SHORTAGE,
    NO_EXP_PRODUCTION_DEMAND_VALUE_SHORTAGE,
    NO_EXP_PRODUCTION_HIGH_COSTS,
    OPTIMIZER_TIMEOUT,
    OPTIMIZER_MAXITER,
    OPTIMIZER_ROUNDOFF_LIMITED,
};

constexpr std::array<const char*, static_cast<int>(EventType::OPTIMIZER_ROUNDOFF_LIMITED) + 1> EVENT_NAMES = {
    "NO_CONSUMPTION",
    "STORAGE_UNDERRUN",
    "STORAGE_OVERRUN",
    "NO_PRODUCTION_SUPPLY_SHORTAGE",
    "NO_PRODUCTION_DEMAND_QUANTITY_SHORTAGE",
    "NO_PRODUCTION_DEMAND_VALUE_SHORTAGE",
    "NO_PRODUCTION_HIGH_COSTS",
    "NO_EXP_PRODUCTION_SUPPLY_SHORTAGE",
    "NO_EXP_PRODUCTION_DEMAND_QUANTITY_SHORTAGE",
    "NO_EXP_PRODUCTION_DEMAND_VALUE_SHORTAGE",
    "NO_EXP_PRODUCTION_HIGH_COSTS",
    "OPTIMIZER_TIMEOUT",
    "OPTIMIZER_MAXITER",
    "OPTIMIZER_ROUNDOFF_LIMITED",
};

class Model;
class Scenario;
class Output;
class Sector;
class EconomicAgent;

class ModelRun {
  private:
    std::unique_ptr<Model> model_;
    std::unique_ptr<Scenario> scenario_;
    std::vector<std::unique_ptr<Output>> outputs_;
    unsigned int time_ = 0;
    std::size_t duration_ = 0;
    IterationStep step_ = IterationStep::INITIALIZATION;
    bool has_run_ = false;
    std::string settings_string_;
    Time start_time_ = Time(0.0);
    Time stop_time_ = Time(0.0);
    std::string basedate_;

  private:
    void step(const IterationStep& step) { step_ = step; }

  public:
    explicit ModelRun(const settings::SettingsNode& settings);
    ~ModelRun();
    void run();
    IterationStep step() const { return step_; }
    unsigned int time() const { return time_; }
    const Time& start_time() const { return start_time_; };
    const Time& stop_time() const { return stop_time_; };
    bool done() const;
    std::size_t duration() const { return duration_; }
    const std::string& settings_string() const { return settings_string_; }
    static unsigned int thread_count();
    std::string timeinfo() const;
    const Output* output(const IndexType i) const { return outputs_[i].get(); }
    static std::string now();
    void event(EventType type, const EconomicAgent* economic_agent, FloatType value = std::numeric_limits<FloatType>::quiet_NaN());
    void event(EventType type, const Sector* sector, const EconomicAgent* economic_agent, FloatType value = std::numeric_limits<FloatType>::quiet_NaN());
    void event(EventType type,
               const EconomicAgent* economic_agent_from,
               const EconomicAgent* economic_agent_to,
               FloatType value = std::numeric_limits<FloatType>::quiet_NaN());

    std::size_t total_timestep_count() const;
    std::string calendar() const;
    std::string basedate() const { return basedate_; }

    const char* name() const { return "RUN"; }
    const Model* model() const { return model_.get(); }
};

}  // namespace acclimate

#endif
