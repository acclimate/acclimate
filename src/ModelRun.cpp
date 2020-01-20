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

#include "ModelRun.h"

#include <cfenv>
#include <chrono>
#include <csignal>
#include <ostream>
#include <string>
#include <utility>

#include "checkpointing.h"
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

static void handle_fpe_error(int /* signal */) {
    if constexpr (options::FLOATING_POINT_EXCEPTIONS) {
        unsigned int exceptions = fetestexcept(FE_ALL_EXCEPT);  // NOLINT(hicpp-signed-bitwise)
        feclearexcept(FE_ALL_EXCEPT);                           // NOLINT(hicpp-signed-bitwise)
        if (exceptions == 0) {
            return;
        }
        if ((exceptions & FE_OVERFLOW) != 0) {  // NOLINT(hicpp-signed-bitwise)
            warning_("FPE_OVERFLOW");
        }
        if ((exceptions & FE_INVALID) != 0) {  // NOLINT(hicpp-signed-bitwise)
            warning_("FPE_INVALID");
        }
        if ((exceptions & FE_DIVBYZERO) != 0) {  // NOLINT(hicpp-signed-bitwise)
            warning_("FPE_DIVBYZERO");
        }
        if ((exceptions & FE_INEXACT) != 0) {  // NOLINT(hicpp-signed-bitwise)
            warning_("FE_INEXACT");
        }
        if ((exceptions & FE_UNDERFLOW) != 0) {  // NOLINT(hicpp-signed-bitwise)
            warning_("FE_UNDERFLOW");
        }
        if constexpr (options::FATAL_FLOATING_POINT_EXCEPTIONS) {
            error_("Floating point exception");
        }
    }
}

ModelRun::ModelRun(const settings::SettingsNode& settings) {
    step(IterationStep::INITIALIZATION);

    if constexpr (options::BANKERS_ROUNDING) {
        fesetround(FE_TONEAREST);
    }

    if constexpr (options::FLOATING_POINT_EXCEPTIONS) {
        signal(SIGFPE, handle_fpe_error);
        feenableexcept(FE_OVERFLOW | FE_INVALID | FE_DIVBYZERO);  // NOLINT(hicpp-signed-bitwise)
    }

    if constexpr (options::CHECKPOINTING) {
        checkpoint::initialize();
    }

    auto model = new Model(this);
    {
        ModelInitializer model_initializer(model, settings);
        model_initializer.initialize();
        if constexpr (options::DEBUGGING) {
            model_initializer.print_network_characteristics();
        }
        model_m.reset(model);
    }

    for (const auto& scenario_node : settings["scenarios"].as_sequence()) {
        Scenario* scenario = nullptr;
        const auto& type = scenario_node["type"].template as<hstring>();
        switch (type) {
            case hstring::hash("events"):
                scenario = new Scenario(settings, scenario_node, model);
                break;
            case hstring::hash("taxes"):
                scenario = new Taxes(settings, scenario_node, model);
                break;
            case hstring::hash("flooding"):
                scenario = new Flooding(settings, scenario_node, model);
                break;
            case hstring::hash("hurricanes"):
                scenario = new Hurricanes(settings, scenario_node, model);
                break;
            case hstring::hash("direct_population"):
                scenario = new DirectPopulation(settings, scenario_node, model);
                break;
            case hstring::hash("heat_labor_productivity"):
                scenario = new HeatLaborProductivity(settings, scenario_node, model);
                break;
            case hstring::hash("event_series"):
                scenario = new EventSeriesScenario(settings, scenario_node, model);
                break;
            default:
                error_("Unknown scenario type '" << type << "'");
        }
        scenarios_m.emplace_back(scenario);
    }

    for (const auto& node : settings["outputs"].as_sequence()) {
        Output* output = nullptr;
        const auto& type = node["format"].template as<hstring>();
        switch (type) {
            case hstring::hash("console"):
                output = new ConsoleOutput(settings, model, node);
                break;
            case hstring::hash("json"):
                output = new JSONOutput(settings, model, node);
                break;
            case hstring::hash("json_network"):
                output = new JSONNetworkOutput(settings, model, node);
                break;
            case hstring::hash("netcdf"):
                output = new NetCDFOutput(settings, model, node);
                break;
            case hstring::hash("histogram"):
                output = new HistogramOutput(settings, model, node);
                break;
            case hstring::hash("gnuplot"):
                output = new GnuplotOutput(settings, model, node);
                break;
            case hstring::hash("damage"):
                output = new DamageOutput(settings, model, node);
                break;
            case hstring::hash("array"):
                output = new ArrayOutput(settings, model, node);
                break;
            case hstring::hash("progress"):
                output = new ProgressOutput(settings, model, node);
                break;
            default:
                error_("Unknown output format '" << type << "'");
        }
        output->initialize();
        outputs_m.emplace_back(output);
    }
}

void ModelRun::run() {
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

        if constexpr (options::CHECKPOINTING) {
            if (checkpoint::is_scheduled) {
                info_("Writing checkpoint");
                for (const auto& output : outputs_m) {
                    output->checkpoint_stop();
                }

                checkpoint::write();

                info_("Resuming from checkpoint");
                t0 = std::chrono::high_resolution_clock::now();
                for (const auto& output : outputs_m) {
                    output->checkpoint_resume();
                }
            }
        }

        step(IterationStep::SCENARIO);
        model_m->tick();
        ++time_m;
    }
}

ModelRun::~ModelRun() {
    if (!options::CHECKPOINTING || !checkpoint::is_scheduled) {
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

void ModelRun::event(EventType type, const Sector* sector_from, const Region* region_from, const Sector* sector_to, const Region* region_to, FloatType value) {
    info_(EVENT_NAMES[(int)type] << " " << (sector_from == nullptr ? "" : sector_from->id()) << (sector_from != nullptr && region_from != nullptr ? ":" : "")
                                 << (region_from == nullptr ? "" : region_from->id()) << "->" << (sector_to == nullptr ? "" : sector_to->id())
                                 << (sector_to != nullptr && region_to != nullptr ? ":" : "") << (region_to == nullptr ? "" : region_to->id())
                                 << (std::isnan(value) ? "" : " = " + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, sector_from, region_from, sector_to, region_to, value);
    }
}

void ModelRun::event(EventType type, const Sector* sector_from, const Region* region_from, const EconomicAgent* economic_agent_to, FloatType value) {
    info_(EVENT_NAMES[(int)type] << " " << (sector_from == nullptr ? "" : sector_from->id()) << (sector_from != nullptr && region_from != nullptr ? ":" : "")
                                 << (region_from == nullptr ? "" : region_from->id()) << (economic_agent_to == nullptr ? "" : "->" + economic_agent_to->id())
                                 << (std::isnan(value) ? "" : " = " + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, sector_from, region_from, economic_agent_to, value);
    }
}

void ModelRun::event(EventType type, const EconomicAgent* economic_agent_from, const EconomicAgent* economic_agent_to, FloatType value) {
    info_(EVENT_NAMES[(int)type] << " " << (economic_agent_from == nullptr ? "" : economic_agent_from->id())
                                 << (economic_agent_to == nullptr ? "" : "->" + economic_agent_to->id())
                                 << (std::isnan(value) ? "" : " = " + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, economic_agent_from, economic_agent_to, value);
    }
}

void ModelRun::event(EventType type, const EconomicAgent* economic_agent_from, const Sector* sector_to, const Region* region_to, FloatType value) {
    info_(EVENT_NAMES[(int)type] << " " << (economic_agent_from == nullptr ? "" : economic_agent_from->id() + "->")
                                 << (sector_to == nullptr ? "" : sector_to->id() + ":") << (region_to == nullptr ? "" : region_to->id())
                                 << (std::isnan(value) ? "" : " = " + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, economic_agent_from, sector_to, region_to, value);
    }
}

unsigned int ModelRun::thread_count() const {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

std::string ModelRun::timeinfo() const {
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
