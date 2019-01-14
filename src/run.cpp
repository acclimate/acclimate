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

#include "run.h"
#include <chrono>
#include <utility>
#ifdef ENABLE_DMTCP
#include <dmtcp.h>
#include <thread>
#endif
#ifdef _OPENMP
#include <omp.h>
#endif
#include "input/ModelInitializer.h"
#include "model/Model.h"
#include "output/ArrayOutput.h"
#include "output/ConsoleOutput.h"
#include "output/DamageOutput.h"
#include "output/GnuplotOutput.h"
#include "output/HistogramOutput.h"
#include "output/JSONNetworkOutput.h"
#include "output/JSONOutput.h"
#include "output/NetCDFOutput.h"
#include "output/ProgressOutput.h"
#include "scenario/DirectPopulation.h"
#include "scenario/EventSeriesScenario.h"
#include "scenario/Flooding.h"
#include "scenario/HeatLaborProductivity.h"
#include "scenario/Hurricanes.h"
#include "scenario/Scenario.h"
#include "scenario/Taxes.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Run<ModelVariant>::Run(settings::SettingsNode settings_p) : settings_m(std::move(settings_p)) {
    step(IterationStep::INITIALIZATION);
    auto model = new Model<ModelVariant>(this);
    {
        ModelInitializer<ModelVariant> model_initializer(model, settings_m);
        model_initializer.initialize();
#ifdef DEBUG
        model_initializer.print_network_characteristics();
#endif
        model_m.reset(model);
    }

    Scenario<ModelVariant>* scenario;  // TODO put in scope below!
    for (auto scenario_node : settings_m["scenarios"].as_sequence()) {
        const std::string& type = scenario_node["type"].as<std::string>();
        if (type == "events") {
            scenario = new Scenario<ModelVariant>(settings_m, scenario_node, model);
        } else if (type == "taxes") {
            scenario = new Taxes<ModelVariant>(settings_m, scenario_node, model);
        } else if (type == "flooding") {
            scenario = new Flooding<ModelVariant>(settings_m, scenario_node, model);
        } else if (type == "hurricanes") {
            scenario = new Hurricanes<ModelVariant>(settings_m, scenario_node, model);
        } else if (type == "direct_population") {
            scenario = new DirectPopulation<ModelVariant>(settings_m, scenario_node, model);
        } else if (type == "heat_labor_productivity") {
            scenario = new HeatLaborProductivity<ModelVariant>(settings_m, scenario_node, model);
        } else if (type == "event_series") {
            scenario = new EventSeriesScenario<ModelVariant>(settings_m, scenario_node, model);
        } else {
            error_("Unknown scenario type '" << type << "'");
        }
        scenarios_m.emplace_back(scenario);
    }

    for (const auto& node : settings_m["outputs"].as_sequence()) {
        Output<ModelVariant>* output;
        const std::string& type = node["format"].template as<std::string>();
        if (type == "console") {
            output = new ConsoleOutput<ModelVariant>(settings_m, model, scenario, node);
        } else if (type == "json") {
            output = new JSONOutput<ModelVariant>(settings_m, model, scenario, node);
        } else if (type == "json_network") {
            output = new JSONNetworkOutput<ModelVariant>(settings_m, model, scenario, node);
        } else if (type == "netcdf") {
            output = new NetCDFOutput<ModelVariant>(settings_m, model, scenario, node);
        } else if (type == "histogram") {
            output = new HistogramOutput<ModelVariant>(settings_m, model, scenario, node);
        } else if (type == "gnuplot") {
            output = new GnuplotOutput<ModelVariant>(settings_m, model, scenario, node);
        } else if (type == "damage") {
            output = new DamageOutput<ModelVariant>(settings_m, model, scenario, node);
        } else if (type == "array") {
            output = new ArrayOutput<ModelVariant>(settings_m, model, scenario, node);
        } else if (type == "progress") {
            output = new ProgressOutput<ModelVariant>(settings_m, model, scenario, node);
        } else {
            error_("Unknown output format '" << type << "'");
        }
        output->initialize();
        outputs_m.emplace_back(output);
    }
}

template<class ModelVariant>
int Run<ModelVariant>::run() {
    if (has_run) {
        error_("model has already run");
    }

    info_("Starting model run on max. " << thread_count() << " threads");

    step(IterationStep::INITIALIZATION);

    for (const auto& scenario : scenarios_m) {
        scenario->start();
    }
    model_m->start();
    for (const auto& output : outputs_m) {
        output->start();
    }
    time_m = 0;

    step(IterationStep::SCENARIO);
#ifdef ENABLE_DMTCP
    auto dmtcp_timer = std::chrono::system_clock::now();
    if (settings_m.has("checkpoint")) {
        if (!dmtcp_is_enabled()) {
            error_("dmtcp is not enabled");
        }
    }
#else
    if (settings_m.has("checkpoint")) {
        error_("dmtcp is not available in this binary");
    }
#endif
    auto t0 = std::chrono::high_resolution_clock::now();

    while (!model_m->done()) {
        for (const auto& scenario : scenarios_m) {
            scenario->iterate();
        }
#ifdef ENABLE_DMTCP
        if (settings_m.has("checkpoint")) {
            if (settings_m["checkpoint"].template as<int>()
                < std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - dmtcp_timer).count()) {
                if (!dmtcp_is_enabled()) {
                    error_("dmtcp is not enabled");
                }
                std::size_t original_generation = dmtcp_get_generation();
                info_("Writing checkpoint");
                for (const auto& output : outputs_m) {
                    output->checkpoint_stop();
                }
                int retval = dmtcp_checkpoint();
                if (retval == DMTCP_AFTER_CHECKPOINT) {
                    do {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    } while (dmtcp_get_generation() == original_generation);
                    run_result = 7;
                    return run_result;
                } else if (retval == DMTCP_AFTER_RESTART) {
                    info_("Resuming from checkpoint");
                    dmtcp_timer = std::chrono::system_clock::now();
                    t0 = std::chrono::high_resolution_clock::now();
                    for (const auto& output : outputs_m) {
                        output->checkpoint_resume();
                    }
                } else if (retval == DMTCP_NOT_PRESENT) {
                    error_("dmtcp not present");
                } else {
                    error_("unknown dmtcp result " << retval);
                }
            }
        }
#endif
        info_("Iteration started");

        model_m->switch_registers();

        step(IterationStep::CONSUMPTION_AND_PRODUCTION);
        model_m->iterate_consumption_and_production();

        step(IterationStep::EXPECTATION);
        model_m->iterate_expectation();

        step(IterationStep::PURCHASE);
        model_m->iterate_purchase();

        step(IterationStep::INVESTMENT);
        model_m->iterate_investment();

        auto t1 = std::chrono::high_resolution_clock::now();
        duration_m = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        t0 = t1;

        step(IterationStep::OUTPUT);
        info_("Iteration took " << duration_m << " ms");
        for (const auto& output : outputs_m) {
            output->iterate();
        }

#ifdef DEBUG
        auto t2 = std::chrono::high_resolution_clock::now();
        info_("Output took " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t0).count() << " ms");
        t0 = t2;
#endif

        step(IterationStep::SCENARIO);
        model_m->tick();
        ++time_m;
    }
    run_result = 0;
    return run_result;
}

template<class ModelVariant>
Run<ModelVariant>::~Run() {
    if (has_run && run_result == 0) {
        for (auto& scenario : scenarios_m) {
            scenario->end();
        }
        for (auto& output : outputs_m) {
            output->end();
        }
    }
    outputs_m.clear();
    scenarios_m.clear();
    model_m.reset();
}

template<class ModelVariant>
void Run<ModelVariant>::event(EventType type,
                              const Sector<ModelVariant>* sector_from,
                              const Region<ModelVariant>* region_from,
                              const Sector<ModelVariant>* sector_to,
                              const Region<ModelVariant>* region_to,
                              FloatType value) {
    info_(event_names[(int)type] << " " << (sector_from == nullptr ? "" : sector_from->id()) << (sector_from != nullptr && region_from != nullptr ? ":" : "")
                                 << (region_from == nullptr ? "" : region_from->id()) << "->" << (sector_to == nullptr ? "" : sector_to->id())
                                 << (sector_to != nullptr && region_to != nullptr ? ":" : "") << (region_to == nullptr ? "" : region_to->id())
                                 << (std::isnan(value) ? "" : " = " + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, sector_from, region_from, sector_to, region_to, value);
    }
}

template<class ModelVariant>
void Run<ModelVariant>::event(EventType type,
                              const Sector<ModelVariant>* sector_from,
                              const Region<ModelVariant>* region_from,
                              const EconomicAgent<ModelVariant>* economic_agent_to,
                              FloatType value) {
    info_(event_names[(int)type] << " " << (sector_from == nullptr ? "" : sector_from->id()) << (sector_from != nullptr && region_from != nullptr ? ":" : "")
                                 << (region_from == nullptr ? "" : region_from->id()) << (economic_agent_to == nullptr ? "" : "->" + economic_agent_to->id())
                                 << (std::isnan(value) ? "" : " = " + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, sector_from, region_from, economic_agent_to, value);
    }
}

template<class ModelVariant>
void Run<ModelVariant>::event(EventType type,
                              const EconomicAgent<ModelVariant>* economic_agent_from,
                              const EconomicAgent<ModelVariant>* economic_agent_to,
                              FloatType value) {
    info_(event_names[(int)type] << " " << (economic_agent_from == nullptr ? "" : economic_agent_from->id())
                                 << (economic_agent_to == nullptr ? "" : "->" + economic_agent_to->id())
                                 << (std::isnan(value) ? "" : " = " + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, economic_agent_from, economic_agent_to, value);
    }
}

template<class ModelVariant>
void Run<ModelVariant>::event(EventType type,
                              const EconomicAgent<ModelVariant>* economic_agent_from,
                              const Sector<ModelVariant>* sector_to,
                              const Region<ModelVariant>* region_to,
                              FloatType value) {
    info_(event_names[(int)type] << " " << (economic_agent_from == nullptr ? "" : economic_agent_from->id() + "->")
                                 << (sector_to == nullptr ? "" : sector_to->id() + ":") << (region_to == nullptr ? "" : region_to->id())
                                 << (std::isnan(value) ? "" : " = " + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, economic_agent_from, sector_to, region_to, value);
    }
}

template<class ModelVariant>
unsigned int Run<ModelVariant>::thread_count() const {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

#ifdef DEBUG
template<class ModelVariant>
std::string Run<ModelVariant>::timeinfo() const {
    std::string res;
    if (step_m != IterationStep::INITIALIZATION) {
        res = std::to_string(time_m) + " ";
    } else {
        res = "  ";
    }
    switch (step_m) {
        case IterationStep::INITIALIZATION:
            res += "INI";
            break;
        case IterationStep::SCENARIO:
            res += "SCE";
            break;
        case IterationStep::CONSUMPTION_AND_PRODUCTION:
            res += "CAP";
            break;
        case IterationStep::EXPECTATION:
            res += "EXP";
            break;
        case IterationStep::PURCHASE:
            res += "PUR";
            break;
        case IterationStep::INVESTMENT:
            res += "INV";
            break;
        case IterationStep::OUTPUT:
            res += "OUT";
            break;
        case IterationStep::CLEANUP:
            res += "CLU";
            break;
        default:
            res += "???";
            break;
    }
    return res;
}
#endif

#define ADD_EVENT(e) __STRING(e),
template<class ModelVariant>
const std::array<const char*, static_cast<int>(EventType::OPTIMIZER_FAILURE) + 1> Run<ModelVariant>::event_names = {ACCLIMATE_ADD_EVENTS};
#undef ADD_EVENT

INSTANTIATE_BASIC(Run);
}  // namespace acclimate
