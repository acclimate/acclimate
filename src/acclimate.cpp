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

#include "acclimate.h"
#include <cfenv>
#include <chrono>
#include <csignal>
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
#include "output/Output.h"
#include "scenario/DirectPopulation.h"
#include "scenario/EventSeriesScenario.h"
#include "scenario/Flooding.h"
#include "scenario/Hurricanes.h"
#include "scenario/Scenario.h"
#include "scenario/Taxes.h"
#include "variants/ModelVariants.h"

namespace acclimate {

#ifdef FLOATING_POINT_EXCEPTIONS
template<class ModelVariant>
static void handle_fpe_error(int sig) {
    UNUSED(sig);
    int exceptions = fetestexcept(FE_ALL_EXCEPT);
    feclearexcept(FE_ALL_EXCEPT);
    if (exceptions == 0) {
        return;
    }
    if (exceptions & FE_OVERFLOW) {
        Acclimate::Run<ModelVariant>::instance()->event(EventType::FPE_OVERFLOW);
    }
    if (exceptions & FE_INVALID) {
        Acclimate::Run<ModelVariant>::instance()->event(EventType::FPE_INVALID);
    }
    if (exceptions & FE_DIVBYZERO) {
        Acclimate::Run<ModelVariant>::instance()->event(EventType::FPE_DIVBYZERO);
    }
    if (exceptions & FE_INEXACT) {
        warning_("FE_INEXACT");
    }
    if (exceptions & FE_UNDERFLOW) {
        warning_("FE_UNDERFLOW");
    }
#ifdef FATAL_FLOATING_POINT_EXCEPTIONS
    error("Floating point exception");
#endif
}
#endif

template<class ModelVariant>
void Acclimate::Run<ModelVariant>::initialize() {
    instance_m.reset(new Run());
}

template<class ModelVariant>
Acclimate::Run<ModelVariant>* Acclimate::Run<ModelVariant>::instance() {
    return instance_m.get();
}

template<class ModelVariant>
Acclimate::Run<ModelVariant>::Run() {
#ifdef BANKERS_ROUNDING
    fesetround(FE_TONEAREST);
#endif
    const settings::SettingsNode& settings = Acclimate::instance()->settings;
    Acclimate::instance()->step(IterationStep::INITIALIZATION);
    auto model = new Model<ModelVariant>();
    {
        ModelInitializer<ModelVariant> model_initializer(model, settings);
        model_initializer.initialize();
#ifdef DEBUG
        model_initializer.print_network_characteristics();
#endif
        model_m.reset(model);
    }

    Scenario<ModelVariant>* scenario;
    {
        const std::string& type = settings["scenario"]["type"].as<std::string>();
        if (type == "events") {
            scenario = new Scenario<ModelVariant>(settings, model);
        } else if (type == "taxes") {
            scenario = new Taxes<ModelVariant>(settings, model);
        } else if (type == "flooding") {
            scenario = new Flooding<ModelVariant>(settings, model);
        } else if (type == "hurricanes") {
            scenario = new Hurricanes<ModelVariant>(settings, model);
        } else if (type == "direct_population") {
            scenario = new DirectPopulation<ModelVariant>(settings, model);
        } else if (type == "event_series") {
            scenario = new EventSeriesScenario<ModelVariant>(settings, model);
        } else {
            error_("Unknown scenario type '" << type << "'");
        }
        scenario_m.reset(scenario);
    }

    for (const auto& node : settings["outputs"].as_sequence()) {
        Output<ModelVariant>* output;
        const std::string& type = node["format"].as<std::string>();
        if (type == "console") {
            output = new ConsoleOutput<ModelVariant>(settings, model, scenario, node);
        } else if (type == "json") {
            output = new JSONOutput<ModelVariant>(settings, model, scenario, node);
        } else if (type == "json_network") {
            output = new JSONNetworkOutput<ModelVariant>(settings, model, scenario, node);
        } else if (type == "netcdf") {
            output = new NetCDFOutput<ModelVariant>(settings, model, scenario, node);
        } else if (type == "histogram") {
            output = new HistogramOutput<ModelVariant>(settings, model, scenario, node);
        } else if (type == "gnuplot") {
            output = new GnuplotOutput<ModelVariant>(settings, model, scenario, node);
        } else if (type == "damage") {
            output = new DamageOutput<ModelVariant>(settings, model, scenario, node);
        } else if (type == "array") {
            output = new ArrayOutput<ModelVariant>(settings, model, scenario, node);
        } else {
            error_("Unknown output format '" << type << "'");
        }
        output->initialize();
        outputs_m.emplace_back(output);
    }

#ifdef FLOATING_POINT_EXCEPTIONS
    signal(SIGFPE, handle_fpe_error<ModelVariant>);
    feenableexcept(FE_OVERFLOW | FE_INVALID | FE_DIVBYZERO);
#endif
}

template<class ModelVariant>
void Acclimate::Run<ModelVariant>::run() {
    info_("Starting model run on max. " << Acclimate::instance()->thread_count() << " threads");

    Acclimate::instance()->step(IterationStep::INITIALIZATION);

    const Time start_time = scenario_m->start();
    model_m->start(start_time);
    for (const auto& output : outputs_m) {
        output->start();
    }
    Acclimate::instance()->time_m = 0;

    Acclimate::instance()->step(IterationStep::SCENARIO);
    auto t0 = std::chrono::high_resolution_clock::now();

    while (scenario_m->iterate()) {
        info_("Iteration started");

        model_m->switch_registers();

        Acclimate::instance()->step(IterationStep::CONSUMPTION_AND_PRODUCTION);
        model_m->iterate_consumption_and_production();

        Acclimate::instance()->step(IterationStep::EXPECTATION);
        model_m->iterate_expectation();

        Acclimate::instance()->step(IterationStep::PURCHASE);
        model_m->iterate_purchase();

        Acclimate::instance()->step(IterationStep::INVESTMENT);
        model_m->iterate_investment();

        auto t1 = std::chrono::high_resolution_clock::now();
        Acclimate::instance()->duration_m = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        t0 = t1;

        Acclimate::instance()->step(IterationStep::OUTPUT);
        info_("Iteration took " << Acclimate::instance()->duration_m << " ms");
        for (const auto& output : outputs_m) {
            output->iterate();
        }

#ifdef DEBUG
        auto t2 = std::chrono::high_resolution_clock::now();
        info_("Output took " << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t0).count() << " ms");
        t0 = t2;
#endif

        Acclimate::instance()->step(IterationStep::SCENARIO);
        model_m->tick();
        ++Acclimate::instance()->time_m;
    }
}

template<class ModelVariant>
void Acclimate::Run<ModelVariant>::cleanup() {
    Acclimate::instance()->step(IterationStep::CLEANUP);
    scenario_m->end();
    for (auto& output : outputs_m) {
        output->end();
    }
    outputs_m.clear();
    scenario_m.reset();
    model_m.reset();
}

template<class ModelVariant>
void Acclimate::Run<ModelVariant>::event(const EventType& type,
                                         const Sector<ModelVariant>* sector_from,
                                         const Region<ModelVariant>* region_from,
                                         const Sector<ModelVariant>* sector_to,
                                         const Region<ModelVariant>* region_to,
                                         const FloatType& value) {
    info_(event_names[(int)type] << " " << (sector_from == nullptr ? "" : std::string(*sector_from))
                                 << (sector_from != nullptr && region_from != nullptr ? ":" : "") << (region_from == nullptr ? "" : std::string(*region_from))
                                 << "->" << (sector_to == nullptr ? "" : std::string(*sector_to)) << (sector_to != nullptr && region_to != nullptr ? ":" : "")
                                 << (region_to == nullptr ? "" : std::string(*region_to))
                                 << (std::isnan(value) ? "" : std::string(" = ") + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, sector_from, region_from, sector_to, region_to, value);
    }
}

template<class ModelVariant>
void Acclimate::Run<ModelVariant>::event(const EventType& type,
                                         const Sector<ModelVariant>* sector_from,
                                         const Region<ModelVariant>* region_from,
                                         const EconomicAgent<ModelVariant>* economic_agent_to,
                                         const FloatType& value) {
    info_(event_names[(int)type] << " " << (sector_from == nullptr ? "" : std::string(*sector_from))
                                 << (sector_from != nullptr && region_from != nullptr ? ":" : "") << (region_from == nullptr ? "" : std::string(*region_from))
                                 << (economic_agent_to == nullptr ? "" : "->" + std::string(*economic_agent_to))
                                 << (std::isnan(value) ? "" : std::string(" = ") + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, sector_from, region_from, economic_agent_to, value);
    }
}

template<class ModelVariant>
void Acclimate::Run<ModelVariant>::event(const EventType& type,
                                         const EconomicAgent<ModelVariant>* economic_agent_from,
                                         const EconomicAgent<ModelVariant>* economic_agent_to,
                                         const FloatType& value) {
    info_(event_names[(int)type] << " " << (economic_agent_from == nullptr ? "" : std::string(*economic_agent_from))
                                 << (economic_agent_to == nullptr ? "" : "->" + std::string(*economic_agent_to))
                                 << (std::isnan(value) ? "" : std::string(" = ") + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, economic_agent_from, economic_agent_to, value);
    }
}

template<class ModelVariant>
void Acclimate::Run<ModelVariant>::event(const EventType& type,
                                         const EconomicAgent<ModelVariant>* economic_agent_from,
                                         const Sector<ModelVariant>* sector_to,
                                         const Region<ModelVariant>* region_to,
                                         const FloatType& value) {
    info_(event_names[(int)type] << " " << (economic_agent_from == nullptr ? "" : std::string(*economic_agent_from) + "->")
                                 << (sector_to == nullptr ? "" : std::string(*sector_to) + ":") << (region_to == nullptr ? "" : std::string(*region_to))
                                 << (std::isnan(value) ? "" : std::string(" = ") + std::to_string(value)));
    for (const auto& output : outputs_m) {
        output->event(type, economic_agent_from, sector_to, region_to, value);
    }
}

Acclimate* Acclimate::instance() { return instance_m.get(); }

unsigned int Acclimate::thread_count() {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

std::string Acclimate::timeinfo() {
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

void Acclimate::initialize(const settings::SettingsNode& settings_p) {
    const std::string& variant = settings_p["variant"].as<std::string>();
    if (variant == "basic") {
        instance_m.reset(new Acclimate(settings_p, ModelVariantType::BASIC));
        Acclimate::Run<VariantBasic>::initialize();
    } else if (variant == "demand") {
        instance_m.reset(new Acclimate(settings_p, ModelVariantType::DEMAND));
        Acclimate::Run<VariantDemand>::initialize();
    } else if (variant == "prices") {
        instance_m.reset(new Acclimate(settings_p, ModelVariantType::PRICES));
        Acclimate::Run<VariantPrices>::initialize();
    } else {
        error_("Unknown model variant '" << variant << "'");
    }
}

void Acclimate::run() {
    switch (variant_m) {
        case ModelVariantType::BASIC:
            Acclimate::Run<VariantBasic>::instance()->run();
            break;
        case ModelVariantType::DEMAND:
            Acclimate::Run<VariantDemand>::instance()->run();
            break;
        case ModelVariantType::PRICES:
            Acclimate::Run<VariantPrices>::instance()->run();
            break;
    }
}

void Acclimate::cleanup() {
    switch (variant_m) {
        case ModelVariantType::BASIC:
            Acclimate::Run<VariantBasic>::instance()->cleanup();
            break;
        case ModelVariantType::DEMAND:
            Acclimate::Run<VariantDemand>::instance()->cleanup();
            break;
        case ModelVariantType::PRICES:
            Acclimate::Run<VariantPrices>::instance()->cleanup();
            break;
    }
}

std::unique_ptr<Acclimate> Acclimate::instance_m;
template<class ModelVariant>
std::unique_ptr<Acclimate::Run<ModelVariant>> Acclimate::Run<ModelVariant>::instance_m;

#define ADD_EVENT(e) __STRING(e),
const std::array<const char*, static_cast<int>(EventType::OPTIMIZER_FAILURE) + 1> Acclimate::event_names = {ACCLIMATE_ADD_EVENTS};
#undef ADD_EVENT

INSTANTIATE_BASIC(Acclimate::Run);
}  // namespace acclimate
