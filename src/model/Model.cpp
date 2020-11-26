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

#include "model/Model.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <memory>
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

Model::Model(ModelRun* run_p) : run_m(run_p) {}

Model::~Model() = default;  // needed to use forward declares for std::unique_ptr

void Model::start() {
    time_ = start_time_;
    timestep_ = 0;
    for (const auto& economic_agent : economic_agents) {
        std::transform(std::begin(economic_agent->input_storages), std::end(economic_agent->input_storages), std::back_inserter(purchasing_managers),
                       [](const auto& is) { return std::make_pair(is->purchasing_manager.get(), 0); });
    }
    if constexpr (options::PARALLELIZATION) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(std::begin(purchasing_managers), std::end(purchasing_managers), g);
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
    for (std::size_t i = 0; i < economic_agents.size(); ++i) {  // NOLINT(modernize-loop-convert)
        economic_agents[i]->iterate_consumption_and_production();
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
    for (std::size_t i = 0; i < purchasing_managers.size(); ++i) {  // NOLINT(modernize-loop-convert)
        auto t1 = std::chrono::high_resolution_clock::now();
        auto& p = purchasing_managers[i];
        p.first->iterate_purchase();
        auto t2 = std::chrono::high_resolution_clock::now();
        p.second = (t2 - t1).count();
    }
    if constexpr (options::PARALLELIZATION) {
        std::sort(std::begin(purchasing_managers), std::end(purchasing_managers),
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

void Model::set_delta_t(const Time& delta_t_p) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    delta_t_ = delta_t_p;
}

void Model::set_start_time(const Time& start_time) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    start_time_ = start_time;
}

void Model::set_stop_time(const Time& stop_time) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    stop_time_ = stop_time;
}

void Model::no_self_supply(bool no_self_supply_p) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    no_self_supply_ = no_self_supply_p;
}

Parameters::ModelParameters& Model::parameters_writable() {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    return parameters_;
}

std::string timeinfo(const Model& m) { return m.run()->timeinfo(); }
IterationStep current_step(const Model& m) { return m.run()->step(); }

}  // namespace acclimate
