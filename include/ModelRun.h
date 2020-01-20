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
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "acclimate.h"
#include "settingsnode.h"

namespace acclimate {

enum class IterationStep {
    INITIALIZATION,
    SCENARIO,
    CONSUMPTION_AND_PRODUCTION,
    EXPECTATION,
    PURCHASE,
    INVESTMENT,
    OUTPUT,
    CLEANUP,
    UNDEFINED  // to be used when function is not used yet
};

#define ACCLIMATE_ADD_EVENTS                              \
    ADD_EVENT(NO_CONSUMPTION)                             \
    ADD_EVENT(STORAGE_UNDERRUN)                           \
    ADD_EVENT(STORAGE_OVERRUN)                            \
    ADD_EVENT(NO_PRODUCTION_SUPPLY_SHORTAGE)              \
    ADD_EVENT(NO_PRODUCTION_DEMAND_QUANTITY_SHORTAGE)     \
    ADD_EVENT(NO_PRODUCTION_DEMAND_VALUE_SHORTAGE)        \
    ADD_EVENT(NO_PRODUCTION_HIGH_COSTS)                   \
    ADD_EVENT(NO_EXP_PRODUCTION_SUPPLY_SHORTAGE)          \
    ADD_EVENT(NO_EXP_PRODUCTION_DEMAND_QUANTITY_SHORTAGE) \
    ADD_EVENT(NO_EXP_PRODUCTION_DEMAND_VALUE_SHORTAGE)    \
    ADD_EVENT(NO_EXP_PRODUCTION_HIGH_COSTS)               \
    ADD_EVENT(DEMAND_FULFILL_HISTORY_UNDERFLOW)           \
    ADD_EVENT(OPTIMIZER_TIMEOUT)                          \
    ADD_EVENT(OPTIMIZER_ROUNDOFF_LIMITED)                 \
    ADD_EVENT(OPTIMIZER_FAILURE)

#define ADD_EVENT(e) e,
enum class EventType : unsigned char { ACCLIMATE_ADD_EVENTS };
#undef ADD_EVENT

#define ADD_EVENT(e) __STRING(e),
static constexpr std::array<const char*, static_cast<int>(EventType::OPTIMIZER_FAILURE) + 1> EVENT_NAMES = {ACCLIMATE_ADD_EVENTS};
#undef ADD_EVENT

class Model;
class Scenario;
class Output;
class Region;
class Sector;
class EconomicAgent;

class ModelRun {
    friend class Acclimate;

  private:
    std::unique_ptr<Model> model_m;
    std::vector<std::unique_ptr<Scenario>> scenarios_m;
    std::vector<std::unique_ptr<Output>> outputs_m;
    unsigned int time_m = 0;
    std::size_t duration_m = 0;
    IterationStep step_m = IterationStep::INITIALIZATION;
    bool has_run = false;

  private:
    void step(const IterationStep& step_p) { step_m = step_p; }

  public:
    explicit ModelRun(const settings::SettingsNode& settings);
    ~ModelRun();
    void run();
    IterationStep step() const { return step_m; }
    unsigned int time() const { return time_m; }
    const std::size_t& duration() const { return duration_m; }
    unsigned int thread_count() const;
    std::string timeinfo() const;
    const Model* model() { return model_m.get(); }
    const Output* output(const IndexType i) { return outputs_m[i].get(); }
    void event(EventType type,
               const Sector* sector_from,
               const Region* region_from,
               const Sector* sector_to,
               const Region* region_to,
               FloatType value = std::numeric_limits<FloatType>::quiet_NaN());
    void event(EventType type,
               const Sector* sector_from,
               const Region* region_from,
               const EconomicAgent* economic_agent_to,
               FloatType value = std::numeric_limits<FloatType>::quiet_NaN());
    void event(EventType type,
               const EconomicAgent* economic_agent_from = nullptr,
               const EconomicAgent* economic_agent_to = nullptr,
               FloatType value = std::numeric_limits<FloatType>::quiet_NaN());
    void event(EventType type,
               const EconomicAgent* economic_agent_from,
               const Sector* sector_to,
               const Region* region_to,
               FloatType value = std::numeric_limits<FloatType>::quiet_NaN());
};

}  // namespace acclimate

#endif
