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

#include <unistd.h>

#include <chrono>

#ifdef ENABLE_DMTCP
#include <dmtcp.h>

#include <csignal>
#include <thread>
#endif  // TODO: remove conditionals

#include "input/ModelInitializer.h"
#include "model/EconomicAgent.h"
#include "model/Government.h"
#include "model/Model.h"
#include "model/PurchasingManager.h"
#include "model/Region.h"
#include "model/Storage.h"
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

namespace acclimate {

static bool instantiated = false;
static volatile bool checkpoint_scheduled = false;

#ifdef ENABLE_DMTCP
static void handle_sigterm(int /* signal */) { checkpoint_scheduled = true; }
#endif

Run::Run(const settings::SettingsNode& settings) {
    step(IterationStep::INITIALIZATION);

#ifdef ENABLE_DMTCP
    if (instantiated) {
        error_("Only one run instance supported when checkpointing");
    }
    std::signal(SIGTERM, handle_sigterm);
    close(10);
    close(11);
#endif
    instantiated = true;

    auto model = new Model(this);
    {
        ModelInitializer model_initializer(model, settings);
        model_initializer.initialize();
        if constexpr (options::DEBUGGING) {
            model_initializer.print_network_characteristics();
        }
        model_m.reset(model);
    }

    Scenario* scenario = nullptr;  // TODO put in scope below!
    for (const auto& scenario_node : settings["scenarios"].as_sequence()) {
        const std::string& type = scenario_node["type"].template as<std::string>();
        // TODO use switch
        if (type == "events") {
            scenario = new Scenario(settings, scenario_node, model);
        } else if (type == "taxes") {
            scenario = new Taxes(settings, scenario_node, model);
        } else if (type == "flooding") {
            scenario = new Flooding(settings, scenario_node, model);
        } else if (type == "hurricanes") {
            scenario = new Hurricanes(settings, scenario_node, model);
        } else if (type == "direct_population") {
            scenario = new DirectPopulation(settings, scenario_node, model);
        } else if (type == "heat_labor_productivity") {
            scenario = new HeatLaborProductivity(settings, scenario_node, model);
        } else if (type == "event_series") {
            scenario = new EventSeriesScenario(settings, scenario_node, model);
        } else {
            error_("Unknown scenario type '" << type << "'");
        }
        scenarios_m.emplace_back(scenario);
    }

    for (const auto& node : settings["outputs"].as_sequence()) {
        Output* output = nullptr;
        const std::string& type = node["format"].template as<std::string>();
        // TODO use switch
        if (type == "console") {
            output = new ConsoleOutput(settings, model, scenario, node);
        } else if (type == "json") {
            output = new JSONOutput(settings, model, scenario, node);
        } else if (type == "json_network") {
            output = new JSONNetworkOutput(settings, model, scenario, node);
        } else if (type == "netcdf") {
            output = new NetCDFOutput(settings, model, scenario, node);
        } else if (type == "histogram") {
            output = new HistogramOutput(settings, model, scenario, node);
        } else if (type == "gnuplot") {
            output = new GnuplotOutput(settings, model, scenario, node);
        } else if (type == "damage") {
            output = new DamageOutput(settings, model, scenario, node);
        } else if (type == "array") {
            output = new ArrayOutput(settings, model, scenario, node);
        } else if (type == "progress") {
            output = new ProgressOutput(settings, model, scenario, node);
        } else {
            error_("Unknown output format '" << type << "'");
        }
        output->initialize();
        outputs_m.emplace_back(output);
    }
}

int Run::run() {
    if (has_run) {
        error_("model has already run");
    }
    has_run = true;

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
    auto t0 = std::chrono::high_resolution_clock::now();

    while (!model_m->done()) {
        for (const auto& scenario : scenarios_m) {
            scenario->iterate();
        }
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

        if constexpr (options::DEBUGGING) {
            auto t2 = std::chrono::high_resolution_clock::now();
            info_("Output took " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t0).count() << " ms");
            t0 = t2;
        }

#ifdef ENABLE_DMTCP
        if (checkpoint_scheduled) {
            info_("Writing checkpoint");
            std::size_t original_generation = dmtcp_get_generation();
            for (const auto& output : outputs_m) {
                output->checkpoint_stop();
            }
            int retval = dmtcp_checkpoint();
            switch (retval) {
                case DMTCP_AFTER_CHECKPOINT:
                    do {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    } while (dmtcp_get_generation() == original_generation);
                    return 7;
                case DMTCP_AFTER_RESTART:
                    checkpoint_scheduled = false;
                    info_("Resuming from checkpoint");
                    t0 = std::chrono::high_resolution_clock::now();
                    for (const auto& output : outputs_m) {
                        output->checkpoint_resume();
                    }
                    break;
                case DMTCP_NOT_PRESENT:
                    error_("dmtcp not present");
                default:
                    error_("unknown dmtcp result " << retval);
            }
        }
#endif

        step(IterationStep::SCENARIO);
        model_m->tick();
        ++time_m;
    }
    return 0;
}

Run::~Run() {
    if (!checkpoint_scheduled) {
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
    instantiated = false;
}

void Run::event(EventType type, const Sector* sector_from, const Region* region_from, const Sector* sector_to, const Region* region_to, FloatType value) {
    info_(EVENT_NAMES[(int)type] << " " << (sector_from == nullptr ? "" : sector_from->id()) << (sector_from != nullptr && region_from != nullptr ? ":" : "")
                                 << (region_from == nullptr ? "" : region_from->id()) << "->" << (sector_to == nullptr ? "" : sector_to->id())
                                 << (sector_to != nullptr && region_to != nullptr ? ":" : "") << (region_to == nullptr ? "" : region_to->id())
                                 << (std::isnan(value) ? "" : " = " + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, sector_from, region_from, sector_to, region_to, value);
    }
}

void Run::event(EventType type, const Sector* sector_from, const Region* region_from, const EconomicAgent* economic_agent_to, FloatType value) {
    info_(EVENT_NAMES[(int)type] << " " << (sector_from == nullptr ? "" : sector_from->id()) << (sector_from != nullptr && region_from != nullptr ? ":" : "")
                                 << (region_from == nullptr ? "" : region_from->id()) << (economic_agent_to == nullptr ? "" : "->" + economic_agent_to->id())
                                 << (std::isnan(value) ? "" : " = " + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, sector_from, region_from, economic_agent_to, value);
    }
}

void Run::event(EventType type, const EconomicAgent* economic_agent_from, const EconomicAgent* economic_agent_to, FloatType value) {
    info_(EVENT_NAMES[(int)type] << " " << (economic_agent_from == nullptr ? "" : economic_agent_from->id())
                                 << (economic_agent_to == nullptr ? "" : "->" + economic_agent_to->id())
                                 << (std::isnan(value) ? "" : " = " + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, economic_agent_from, economic_agent_to, value);
    }
}

void Run::event(EventType type, const EconomicAgent* economic_agent_from, const Sector* sector_to, const Region* region_to, FloatType value) {
    info_(EVENT_NAMES[(int)type] << " " << (economic_agent_from == nullptr ? "" : economic_agent_from->id() + "->")
                                 << (sector_to == nullptr ? "" : sector_to->id() + ":") << (region_to == nullptr ? "" : region_to->id())
                                 << (std::isnan(value) ? "" : " = " + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, economic_agent_from, sector_to, region_to, value);
    }
}

unsigned int Run::thread_count() const {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

std::string Run::timeinfo() const {
    if constexpr (options::DEBUGGING) {
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
    return "";
}

}  // namespace acclimate
