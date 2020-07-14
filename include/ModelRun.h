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

#ifndef ACCLIMATE_RUN_H
#define ACCLIMATE_RUN_H

#include <features.h>  // for __STRING

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

#define ACCLIMATE_ADD_EVENTS                                  \
    ADD_ENUM_ITEM(NO_CONSUMPTION)                             \
    ADD_ENUM_ITEM(STORAGE_UNDERRUN)                           \
    ADD_ENUM_ITEM(STORAGE_OVERRUN)                            \
    ADD_ENUM_ITEM(NO_PRODUCTION_SUPPLY_SHORTAGE)              \
    ADD_ENUM_ITEM(NO_PRODUCTION_DEMAND_QUANTITY_SHORTAGE)     \
    ADD_ENUM_ITEM(NO_PRODUCTION_DEMAND_VALUE_SHORTAGE)        \
    ADD_ENUM_ITEM(NO_PRODUCTION_HIGH_COSTS)                   \
    ADD_ENUM_ITEM(NO_EXP_PRODUCTION_SUPPLY_SHORTAGE)          \
    ADD_ENUM_ITEM(NO_EXP_PRODUCTION_DEMAND_QUANTITY_SHORTAGE) \
    ADD_ENUM_ITEM(NO_EXP_PRODUCTION_DEMAND_VALUE_SHORTAGE)    \
    ADD_ENUM_ITEM(NO_EXP_PRODUCTION_HIGH_COSTS)               \
    ADD_ENUM_ITEM(DEMAND_FULFILL_HISTORY_UNDERFLOW)           \
    ADD_ENUM_ITEM(OPTIMIZER_TIMEOUT)                          \
    ADD_ENUM_ITEM(OPTIMIZER_ROUNDOFF_LIMITED)

#define ADD_ENUM_ITEM(e) e,
enum class EventType : unsigned char { ACCLIMATE_ADD_EVENTS };
#undef ADD_ENUM_ITEM

#define ADD_ENUM_ITEM(e) __STRING(e),
constexpr std::array<const char*, static_cast<int>(EventType::OPTIMIZER_ROUNDOFF_LIMITED) + 1> EVENT_NAMES = {ACCLIMATE_ADD_EVENTS};
#undef ADD_ENUM_ITEM

#undef ACCLIMATE_ADD_EVENTS

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
