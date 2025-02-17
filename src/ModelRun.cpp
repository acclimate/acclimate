// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "ModelRun.h"

#include <cfenv>
#include <csignal>

#include "acclimate.h"
#include "checkpointing.h"
#include "input/ModelInitializer.h"
#include "model/EconomicAgent.h"
#include "model/Model.h"
#include "model/Sector.h"
#include "openmp.h"
#include "output/ArrayOutput.h"
#include "output/NetCDFOutput.h"
#include "output/Output.h"
#include "output/ProgressOutput.h"
#include "scenario/EventSeriesScenario.h"
#include "scenario/Scenario.h"
#include "settingsnode.h"

namespace acclimate {

static void handle_fpe_error(int /* signal */) {
    if constexpr (Options::FLOATING_POINT_EXCEPTIONS) {
        unsigned int const exceptions = fetestexcept(FE_ALL_EXCEPT);  // NOLINT(hicpp-signed-bitwise)
        feclearexcept(FE_ALL_EXCEPT);                                 // NOLINT(hicpp-signed-bitwise)
        if (exceptions == 0) {
            return;
        }
        if ((exceptions & FE_OVERFLOW) != 0) {  // NOLINT(hicpp-signed-bitwise)
            log::warning("FPE_OVERFLOW");
        }
        if ((exceptions & FE_INVALID) != 0) {  // NOLINT(hicpp-signed-bitwise)
            log::warning("FPE_INVALID");
        }
        if ((exceptions & FE_DIVBYZERO) != 0) {  // NOLINT(hicpp-signed-bitwise)
            log::warning("FPE_DIVBYZERO");
        }
        if ((exceptions & FE_INEXACT) != 0) {  // NOLINT(hicpp-signed-bitwise)
            log::warning("FE_INEXACT");
        }
        if ((exceptions & FE_UNDERFLOW) != 0) {  // NOLINT(hicpp-signed-bitwise)
            log::warning("FE_UNDERFLOW");
        }
        if constexpr (Options::FATAL_FLOATING_POINT_EXCEPTIONS) {
            throw log::error("Floating point exception");
        }
    }
}

ModelRun::ModelRun(const settings::SettingsNode& settings) {
    step(IterationStep::INITIALIZATION);

    if constexpr (Options::BANKERS_ROUNDING) {
        fesetround(FE_TONEAREST);
    }

    if constexpr (Options::FLOATING_POINT_EXCEPTIONS) {
        signal(SIGFPE, handle_fpe_error);
        feenableexcept(FE_OVERFLOW | FE_INVALID | FE_DIVBYZERO);  // NOLINT(hicpp-signed-bitwise)
    } else {
        (void)handle_fpe_error;
    }

    if constexpr (Options::CHECKPOINTING) {
        checkpoint::initialize();
    }

    {
        std::ostringstream ss;
        ss << settings;
        settings_string_ = ss.str();
    }

    const settings::SettingsNode& scenario_node = settings["scenario"];
    start_time_ = scenario_node["start"].as<Time>();
    stop_time_ = scenario_node["stop"].as<Time>();
    if (scenario_node.has("baseyear") && !scenario_node.has("basedate")) {
        basedate_ = scenario_node["baseyear"].as<std::string>() + "-1-1";
    } else {
        basedate_ = scenario_node["basedate"].as<std::string>("2000-1-1");
    }

    auto* model = new Model(this);
    {
        ModelInitializer model_initializer(model, settings);
        model_initializer.initialize();
        if constexpr (Options::DEBUGGING) {
            model_initializer.debug_print_network_characteristics();
        }
        model_.reset(model);
    }

    {
        const auto& type = scenario_node["type"].as<hashed_string>();
        switch (type) {
            case hash("events"):  // TODO separate
                scenario_ = std::make_unique<Scenario>(settings, scenario_node, model_.get());
                break;
            case hash("event_series"):
                scenario_ = std::make_unique<EventSeriesScenario>(settings, scenario_node, model_.get());
                break;
            default:
                throw log::error("Unknown scenario_ type '", type, "'");
        }
    }

    for (const auto& node : settings["outputs"].as_sequence()) {
        Output* output = nullptr;
        const auto& type = node["format"].as<hashed_string>();
        switch (type) {
            // TODO case hash("watch"):
            case hash("netcdf"):
                output = new NetCDFOutput(model, node);
                break;
            case hash("array"):
                output = new ArrayOutput(model, node, false);
                break;
            case hash("progress"):
                output = new ProgressOutput(model);
                break;
            default:
                throw log::error("Unknown output format '", type, "'");
        }
        outputs_.emplace_back(output);
    }
}

void ModelRun::run() {
    if (has_run_) {
        throw log::error(this, "Model has already run");
    }
    has_run_ = true;

    log::info(this, "Starting model run on max. ", thread_count(), " threads");

    step(IterationStep::INITIALIZATION);

    scenario_->start();
    model_->time_ = start_time_;
    model_->start();
    for (const auto& output : outputs_) {
        output->start();
    }
    time_ = 0;

    step(IterationStep::SCENARIO);
    auto t0 = std::chrono::high_resolution_clock::now();

    while (!done()) {
        scenario_->iterate();
        log::info(this, "Iteration started");

        model_->switch_registers();

        step(IterationStep::CONSUMPTION_AND_PRODUCTION);
        model_->iterate_consumption_and_production();

        step(IterationStep::EXPECTATION);
        model_->iterate_expectation();

        step(IterationStep::PURCHASE);
        model_->iterate_purchase();

        if (model_->parameters().with_investment_dynamics) {
            step(IterationStep::INVESTMENT);
            model_->iterate_investment();
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        duration_ = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        t0 = t1;

        step(IterationStep::OUTPUT);
        log::info(this, "Iteration took ", duration_, " ms");
        for (const auto& output : outputs_) {
            output->iterate();
        }

        if constexpr (Options::DEBUGGING) {
            auto t2 = std::chrono::high_resolution_clock::now();
            log::info(this, "Output took ", std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t0).count(), " ms");
            t0 = t2;
        }

        if constexpr (Options::CHECKPOINTING) {
            if (checkpoint::is_scheduled) {
                log::info(this, "Writing checkpoint");
                for (const auto& output : outputs_) {
                    output->checkpoint_stop();
                }

                checkpoint::write();

                log::info(this, "Resuming from checkpoint");
                t0 = std::chrono::high_resolution_clock::now();
                for (const auto& output : outputs_) {
                    output->checkpoint_resume();
                }
            }
        }

        step(IterationStep::SCENARIO);
        model_->tick();
        ++time_;
    }
}

ModelRun::~ModelRun() {
    if (!Options::CHECKPOINTING || !checkpoint::is_scheduled) {
        scenario_->end();
        for (auto& output : outputs_) {
            output->end();
        }
    }
    outputs_.clear();
    model_.reset();
}

auto ModelRun::done() const -> bool { return model_->time() > stop_time_; }

auto ModelRun::calendar() const -> std::string { return scenario_->calendar_str(); }

auto ModelRun::total_timestep_count() const -> std::size_t { return (stop_time_ - start_time_) / model_->delta_t() + 1; }

auto ModelRun::now() -> std::string {
    std::string res = "0000-00-00 00:00:00";
    auto t = std::time(nullptr);
    std::strftime(res.data(), res.size() + 1, "%F %T", std::localtime(&t));
    return res;
}

void ModelRun::event(EventType type, const Sector* sector, const EconomicAgent* economic_agent, FloatType value) {
    log::info(this, EVENT_NAMES[static_cast<int>(type)], " ", sector->id, "->", economic_agent->id, std::isnan(value) ? "" : " = " + std::to_string(value));
    for (const auto& output : outputs_) {
        output->event(type, sector, economic_agent, value);
    }
}

void ModelRun::event(EventType type, const EconomicAgent* economic_agent, FloatType value) {
    log::info(this, EVENT_NAMES[static_cast<int>(type)], " ", economic_agent->id, std::isnan(value) ? "" : " = " + std::to_string(value));
    for (const auto& output : outputs_) {
        output->event(type, economic_agent, value);
    }
}

void ModelRun::event(EventType type, const EconomicAgent* economic_agent_from, const EconomicAgent* economic_agent_to, FloatType value) {
    log::info(this, EVENT_NAMES[static_cast<int>(type)], " ", economic_agent_from->id, "->", economic_agent_to->id,
              std::isnan(value) ? "" : " = " + std::to_string(value));
    for (const auto& output : outputs_) {
        output->event(type, economic_agent_from, economic_agent_to, value);
    }
}

auto ModelRun::thread_count() -> unsigned int { return openmp::get_thread_count(); }

auto ModelRun::timeinfo() const -> std::string {
    if constexpr (Options::DEBUGGING) {
        std::string res;
        if (step_ != IterationStep::INITIALIZATION) {
            res = std::to_string(time_) + " ";
        } else {
            res = "  ";
        }
        switch (step_) {
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
