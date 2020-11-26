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
    DEMAND_FULFILL_HISTORY_UNDERFLOW,
    OPTIMIZER_TIMEOUT,
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
    "DEMAND_FULFILL_HISTORY_UNDERFLOW",
    "OPTIMIZER_TIMEOUT",
    "OPTIMIZER_ROUNDOFF_LIMITED",
};

class Model;
class Scenario;
class Output;
class Sector;
class EconomicAgent;

class ModelRun {
  private:
    std::unique_ptr<Model> model_m;
    std::vector<std::unique_ptr<Scenario>> scenarios_m;
    std::vector<std::unique_ptr<Output>> outputs_m;
    unsigned int time_m = 0;
    std::size_t duration_m = 0;
    IterationStep step_m = IterationStep::INITIALIZATION;
    bool has_run = false;
    std::string settings_string_m;

  private:
    void step(const IterationStep& step_p) { step_m = step_p; }

  public:
    explicit ModelRun(const settings::SettingsNode& settings);
    ~ModelRun();
    void run();
    IterationStep step() const { return step_m; }
    unsigned int time() const { return time_m; }
    std::size_t duration() const { return duration_m; }
    const std::string& settings_string() const { return settings_string_m; }
    unsigned int thread_count() const;
    std::string timeinfo() const;
    const Output* output(const IndexType i) const { return outputs_m[i].get(); }
    std::string now() const;
    void event(EventType type, const EconomicAgent* economic_agent, FloatType value = std::numeric_limits<FloatType>::quiet_NaN());
    void event(EventType type, const Sector* sector, const EconomicAgent* economic_agent, FloatType value = std::numeric_limits<FloatType>::quiet_NaN());
    void event(EventType type,
               const EconomicAgent* economic_agent_from,
               const EconomicAgent* economic_agent_to,
               FloatType value = std::numeric_limits<FloatType>::quiet_NaN());

    std::size_t total_timestep_count() const { return 1; }  // TODO
    std::string calendar() const { return "standard"; }     // TODO
    int baseyear() const { return 2010; }                   // TODO

    const char* name() const { return "RUN"; }
    const Model* model() const { return model_m.get(); }
};

}  // namespace acclimate

#endif
