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

#include <features.h>
#include <array>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "settingsnode.h"
#include "types.h"

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

template<class ModelVariant>
class Run {
    friend class Acclimate;

  protected:
    static std::unique_ptr<Run> instance_m;
    std::unique_ptr<Model<ModelVariant>> model_m;
    std::vector<std::unique_ptr<Scenario<ModelVariant>>> scenarios_m;
    std::vector<std::unique_ptr<Output<ModelVariant>>> outputs_m;
    unsigned int time_m = 0;
    std::size_t duration_m = 0;
    IterationStep step_m = IterationStep::INITIALIZATION;
    settings::SettingsNode settings_m;
    int run_result = -1;
    bool has_run = false;

    static void initialize();
    int run();
    inline void step(const IterationStep& step_p) { step_m = step_p; }

  public:
    static const std::array<const char*, static_cast<int>(EventType::OPTIMIZER_FAILURE) + 1> event_names;

    Run(settings::SettingsNode settings_p);
    ~Run();
    inline IterationStep step() const { return step_m; }
    inline unsigned int time() const { return time_m; }
    inline const std::size_t& duration() const { return duration_m; }
    unsigned int thread_count() const;
#ifdef DEBUG
    std::string timeinfo() const;
#endif
    inline const Model<ModelVariant>* model() { return model_m.get(); }
    inline const Output<ModelVariant>* output(const IntType i) { return outputs_m[i].get(); }
    void event(EventType type,
               const Sector<ModelVariant>* sector_from,
               const Region<ModelVariant>* region_from,
               const Sector<ModelVariant>* sector_to,
               const Region<ModelVariant>* region_to,
               FloatType value = std::numeric_limits<FloatType>::quiet_NaN());
    void event(EventType type,
               const Sector<ModelVariant>* sector_from,
               const Region<ModelVariant>* region_from,
               const EconomicAgent<ModelVariant>* economic_agent_to,
               FloatType value = std::numeric_limits<FloatType>::quiet_NaN());
    void event(EventType type,
               const EconomicAgent<ModelVariant>* economic_agent_from = nullptr,
               const EconomicAgent<ModelVariant>* economic_agent_to = nullptr,
               FloatType value = std::numeric_limits<FloatType>::quiet_NaN());
    void event(EventType type,
               const EconomicAgent<ModelVariant>* economic_agent_from,
               const Sector<ModelVariant>* sector_to,
               const Region<ModelVariant>* region_to,
               FloatType value = std::numeric_limits<FloatType>::quiet_NaN());
};

#undef assert
#ifdef DEBUG
#define error(a)                                                                        \
    {                                                                                   \
        std::ostringstream ss;                                                          \
        ss << id() << " error: " << a << " (" << __FILE__ << ", l." << __LINE__ << ")"; \
        throw acclimate::exception(ss.str());                                           \
    }
#define error_(a)                                                              \
    {                                                                          \
        std::ostringstream ss;                                                 \
        ss << "error: " << a << " (" << __FILE__ << ", l." << __LINE__ << ")"; \
        throw acclimate::exception(ss.str());                                  \
    }
#define assert_(expr)                                                                                  \
    if (!(expr)) {                                                                                     \
        std::ostringstream ss;                                                                         \
        ss << "assertion failed: " << __STRING(expr) << " (" << __FILE__ << ", l." << __LINE__ << ")"; \
        throw acclimate::exception(ss.str());                                                          \
    }
#define assert(expr)                                                                                            \
    if (!(expr)) {                                                                                              \
        std::ostringstream ss;                                                                                  \
        ss << id() << " assertion failed: " << __STRING(expr) << " (" << __FILE__ << ", l." << __LINE__ << ")"; \
        throw acclimate::exception(ss.str());                                                                   \
    }
#ifndef TEST
#define assertstep(a)                                                   \
    if (model()->run()->step() != IterationStep::a) {                   \
        std::ostringstream ss;                                          \
        ss << id() << " error: should be in " << __STRING(a) << " step" \
           << " (" << __FILE__ << ", l." << __LINE__ << ")";            \
        throw acclimate::exception(ss.str());                           \
    }
#define assertstepor(a, b)                                                                          \
    if (model()->run()->step() != IterationStep::a && model()->run()->step() != IterationStep::b) { \
        std::ostringstream ss;                                                                      \
        ss << id() << " error: should be in " << __STRING(a) << " or " << __STRING(b) << " step"    \
           << " (" << __FILE__ << ", l." << __LINE__ << ")";                                        \
        throw acclimate::exception(ss.str());                                                       \
    }
#define assertstepnot(a)                                                    \
    if (model()->run()->step() == IterationStep::a) {                       \
        std::ostringstream ss;                                              \
        ss << id() << " error: should NOT be in " << __STRING(a) << " step" \
           << " (" << __FILE__ << ", l." << __LINE__ << ")";                \
        throw acclimate::exception(ss.str());                               \
    }
#else
#define assertstep(a) \
    {}
#define assertstepor(a, b) \
    {}
#define assertstepnot(a) \
    {}
#endif
#define warning(a) \
    _Pragma("omp critical (output)") { std::cout << model()->run()->timeinfo() << ", " << id() << ": Warning: " << a << std::endl; }
#define info(a) \
    _Pragma("omp critical (output)") { std::cout << model()->run()->timeinfo() << ", " << id() << ": " << a << std::endl; }
#define debug(a) \
    _Pragma("omp critical (output)") { std::cout << model()->run()->timeinfo() << ", " << id() << ": " << __STRING(a) << " = " << a << std::endl; }
#define errinfo_(a) \
    _Pragma("omp critical (output)") { std::cerr << model()->run()->timeinfo() << ", " << a << std::endl; }
#define warning_(a) \
    _Pragma("omp critical (output)") { std::cout << "Warning: " << a << std::endl; }
#define info_(a) \
    _Pragma("omp critical (output)") { std::cout << a << std::endl; }
#define debug_(a) \
    _Pragma("omp critical (output)") { std::cout << __STRING(a) << " = " << a << std::endl; }

#else  // !def DEBUG

#define error(a)                              \
    {                                         \
        std::ostringstream ss;                \
        ss << a;                              \
        throw acclimate::exception(ss.str()); \
    }
#define error_(a)                             \
    {                                         \
        std::ostringstream ss;                \
        ss << a;                              \
        throw acclimate::exception(ss.str()); \
    }
#define assert_(a) \
    {}
#define assert(a) \
    {}
#define assertstep(a) \
    {}
#define assertstepor(a, b) \
    {}
#define assertstepnot(a) \
    {}
#define warning(a) \
    {}
#define info(a) \
    {}
#define debug(a) \
    {}
#define warning_(a) \
    {}
#define info_(a) \
    {}
#define errinfo_(a) \
    {}
#define debug_(a) \
    {}

#endif

}  // namespace acclimate

#endif
