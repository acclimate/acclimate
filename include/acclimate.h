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

#ifndef ACCLIMATE_H
#define ACCLIMATE_H

#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "helpers.h"
#include "settingsnode.h"
#include "types.h"

namespace acclimate {

#define ACCLIMATE_ADD_EVENTS                              \
    ADD_EVENT(FPE_OVERFLOW)                               \
    ADD_EVENT(FPE_INVALID)                                \
    ADD_EVENT(FPE_DIVBYZERO)                              \
    ADD_EVENT(NO_CONSUMPTION)                             \
    ADD_EVENT(STORAGE_EMPTY)                              \
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

template<class ModelVariant>
class Model;
template<class ModelVariant>
class Scenario;
template<class ModelVariant>
class Output;
template<class ModelVariant>
class Region;
template<class ModelVariant>
class Sector;
template<class ModelVariant>
class EconomicAgent;
enum class ModelVariantType;

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

class Acclimate {
  public:
    template<class ModelVariant>
    class Run {
        friend class Acclimate;

      protected:
        static std::unique_ptr<Run> instance_m;
        std::unique_ptr<Scenario<ModelVariant>> scenario_m;
        std::unique_ptr<Model<ModelVariant>> model_m;
        std::vector<std::unique_ptr<Output<ModelVariant>>> outputs_m;
        static void initialize();
        int run();
        void cleanup();
        void memory_cleanup();
        Run();

      public:
        static Run* instance();
        inline const Model<ModelVariant>* model() { return model_m.get(); }
        inline const Output<ModelVariant>* output(const IntType i) { return outputs_m[i].get(); }
        void event(const EventType& type,
                   const Sector<ModelVariant>* sector_from,
                   const Region<ModelVariant>* region_from,
                   const Sector<ModelVariant>* sector_to,
                   const Region<ModelVariant>* region_to,
                   const FloatType& value = std::numeric_limits<FloatType>::quiet_NaN());
        void event(const EventType& type,
                   const Sector<ModelVariant>* sector_from,
                   const Region<ModelVariant>* region_from,
                   const EconomicAgent<ModelVariant>* economic_agent_to,
                   const FloatType& value = std::numeric_limits<FloatType>::quiet_NaN());
        void event(const EventType& type,
                   const EconomicAgent<ModelVariant>* economic_agent_from = nullptr,
                   const EconomicAgent<ModelVariant>* economic_agent_to = nullptr,
                   const FloatType& value = std::numeric_limits<FloatType>::quiet_NaN());
        void event(const EventType& type,
                   const EconomicAgent<ModelVariant>* economic_agent_from,
                   const Sector<ModelVariant>* sector_to,
                   const Region<ModelVariant>* region_to,
                   const FloatType& value = std::numeric_limits<FloatType>::quiet_NaN());
    };

  protected:
    static std::unique_ptr<Acclimate> instance_m;
    inline void step(const IterationStep& step_p) { step_m = step_p; }
    unsigned int time_m = 0;
    std::size_t duration_m = 0;
    IterationStep step_m = IterationStep::INITIALIZATION;
    settings::SettingsNode settings;
    ModelVariantType variant_m;
    Acclimate(const settings::SettingsNode& settings_p, ModelVariantType variant_p) : settings(std::move(settings_p)), variant_m(variant_p) {}

  public:
    static Acclimate* instance();
    static const std::array<const char*, static_cast<int>(EventType::OPTIMIZER_FAILURE) + 1> event_names;

    inline const ModelVariantType& variant() { return variant_m; }
    inline IterationStep step() { return step_m; }
    inline const std::size_t& duration() { return duration_m; }
    unsigned int thread_count();
    std::string timeinfo();
    static void initialize(const settings::SettingsNode& settings_p);
    int run();
    void cleanup();
    void memory_cleanup();
};
}

#endif
