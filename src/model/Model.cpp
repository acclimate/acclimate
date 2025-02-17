// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/Model.h"

#include <chrono>
#include <random>

#include "ModelRun.h"
#include "acclimate.h"
#include "model/EconomicAgent.h"
#include "model/GeoLocation.h"
#include "model/PurchasingManager.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "model/Storage.h"  // IWYU pragma: keep

namespace acclimate {

Model::Model(ModelRun* run) : run_(run) {}

Model::~Model() = default;  // needed to use forward declares for std::unique_ptr

void Model::start() {
    timestep_ = 0;
    for (const auto& economic_agent : economic_agents) {
        std::transform(std::begin(economic_agent->input_storages), std::end(economic_agent->input_storages), std::back_inserter(parallelized_storages),
                       [](const auto& is) { return std::make_pair(is->purchasing_manager.get(), 0); });
    }
    std::transform(std::begin(economic_agents), std::end(economic_agents), std::back_inserter(parallelized_agents),
                   [](const auto& ea) { return std::make_pair(ea.get(), 0); });
    if constexpr (Options::PARALLELIZATION) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(std::begin(parallelized_storages), std::end(parallelized_storages), g);
        std::shuffle(std::begin(parallelized_agents), std::end(parallelized_agents), g);
    }
}

void Model::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < sectors.size(); ++i) {  // NOLINT(modernize-loop-convert)
        sectors[i]->iterate_consumption_and_production();
    }

#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < regions.size(); ++i) {  // NOLINT(modernize-loop-convert)
        regions[i]->iterate_consumption_and_production();
    }

#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < parallelized_agents.size(); ++i) {  // NOLINT(modernize-loop-convert)
        auto t1 = std::chrono::high_resolution_clock::now();
        auto& p = parallelized_agents[i];
        p.first->iterate_consumption_and_production();
        auto t2 = std::chrono::high_resolution_clock::now();
        p.second = (t2 - t1).count();
    }
    if constexpr (Options::PARALLELIZATION) {
        std::sort(std::begin(parallelized_agents), std::end(parallelized_agents),
                  [](const std::pair<EconomicAgent*, std::size_t>& a, const std::pair<EconomicAgent*, std::size_t>& b) { return b.second > a.second; });
    }
}

void Model::iterate_expectation() {
    debug::assertstep(this, IterationStep::EXPECTATION);
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < regions.size(); ++i) {  // NOLINT(modernize-loop-convert)
        regions[i]->iterate_expectation();
    }

#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < economic_agents.size(); ++i) {  // NOLINT(modernize-loop-convert)
        economic_agents[i]->iterate_expectation();
    }
}

void Model::iterate_purchase() {
    debug::assertstep(this, IterationStep::PURCHASE);
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < regions.size(); ++i) {  // NOLINT(modernize-loop-convert)
        regions[i]->iterate_purchase();
    }
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < parallelized_storages.size(); ++i) {  // NOLINT(modernize-loop-convert)
        auto t1 = std::chrono::high_resolution_clock::now();
        auto& p = parallelized_storages[i];
        p.first->iterate_purchase();
        auto t2 = std::chrono::high_resolution_clock::now();
        p.second = (t2 - t1).count();
    }
    if constexpr (Options::PARALLELIZATION) {
        std::sort(std::begin(parallelized_storages), std::end(parallelized_storages),
                  [](const std::pair<PurchasingManager*, std::size_t>& a, const std::pair<PurchasingManager*, std::size_t>& b) { return b.second > a.second; });
    }
}

void Model::iterate_investment() {
    debug::assertstep(this, IterationStep::INVESTMENT);
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < regions.size(); ++i) {  // NOLINT(modernize-loop-convert)
        regions[i]->iterate_investment();
    }

#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < economic_agents.size(); ++i) {  // NOLINT(modernize-loop-convert)
        economic_agents[i]->iterate_investment();
    }
}

void Model::switch_registers() {
    debug::assertstep(this, IterationStep::SCENARIO);
    current_register_ = 1 - current_register_;
}

void Model::tick() {
    debug::assertstep(this, IterationStep::SCENARIO);
    time_ += delta_t_;
    ++timestep_;
}

void Model::set_delta_t(const Time& delta_t) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    delta_t_ = delta_t;
}

auto timeinfo(const Model& m) -> std::string { return m.run()->timeinfo(); }
auto current_step(const Model& m) -> IterationStep { return m.run()->step(); }

}  // namespace acclimate
