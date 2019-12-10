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
#include <random>
#include "model/Consumer.h"
#include "model/EconomicAgent.h"
#include "model/GeoLocation.h"
#include "model/GeoRoute.h"
#include "model/Government.h"
#include "model/Firm.h"
#include "model/PurchasingManagerPrices.h"
#include "model/Region.h"
#include "model/Storage.h"
#include "run.h"

namespace acclimate {

Model::Model(Run* const run_p)
    : run_m(run_p), consumption_sector(new Sector(this, "FCON", 0, Ratio(0.0), Time(0.0), Sector::TransportType::IMMEDIATE)) {
    sectors.emplace_back(consumption_sector);
}

Region* Model::add_region(std::string name) {
    auto region = new Region(this, name, regions.size());
    regions.emplace_back(region);
    return region;
}

Sector *Model::add_sector(std::string name,
                          const Ratio &upper_storage_limit_omega_p,
                          const Time &initial_storage_fill_factor_psi_p,
                          typename Sector::TransportType transport_type_p) {
    auto sector = new Sector(this, name, sectors.size(), upper_storage_limit_omega_p, initial_storage_fill_factor_psi_p,
                             transport_type_p);
    sectors.emplace_back(sector);
    return sector;
}

void Model::start() {
    time_ = start_time_;
    timestep_ = 0;
    for (const auto& region : regions) {
        for (const auto& economic_agent : region->economic_agents) {
            economic_agents.push_back(std::make_pair(economic_agent.get(), 0));
            for (const auto& is : economic_agent->input_storages) {
                purchasing_managers.push_back(std::make_pair(is->purchasing_manager.get(), 0));
            }
        }
    }
#ifdef _OPENMP
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(std::begin(economic_agents), std::end(economic_agents), g);
    std::shuffle(std::begin(purchasing_managers), std::end(purchasing_managers), g);
#endif
}

void Model::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < sectors.size(); ++i) {
        sectors[i]->iterate_consumption_and_production();
    }

#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < regions.size(); ++i) {
        regions[i]->iterate_consumption_and_production();
    }

#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < economic_agents.size(); ++i) {
        auto& p = economic_agents[i];
        p.first->iterate_consumption_and_production();
    }
}

void Model::iterate_expectation() {
    assertstep(EXPECTATION);
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < regions.size(); ++i) {
        regions[i]->iterate_expectation();
    }

#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < economic_agents.size(); ++i) {
        auto& p = economic_agents[i];
        p.first->iterate_expectation();
    }
}

void Model::iterate_purchase() {
    assertstep(PURCHASE);
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < regions.size(); ++i) {
        regions[i]->iterate_purchase();
    }
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < purchasing_managers.size(); ++i) {
        auto t1 = std::chrono::high_resolution_clock::now();
        auto& p = purchasing_managers[i];
        p.first->iterate_purchase();
        auto t2 = std::chrono::high_resolution_clock::now();
        p.second = (t2 - t1).count();
    }
#ifdef _OPENMP
    std::sort(std::begin(purchasing_managers), std::end(purchasing_managers),
              [](const std::pair<PurchasingManager*, std::size_t>& a, const std::pair<PurchasingManager*, std::size_t>& b) {
                  return b.second > a.second;
              });
#endif
}

void Model::iterate_investment() {
    assertstep(INVESTMENT);
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < regions.size(); ++i) {
        regions[i]->iterate_investment();
    }

#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < economic_agents.size(); ++i) {
        auto& p = economic_agents[i];
        p.first->iterate_investment();
    }
}

Region* Model::find_region(const std::string& name) const {
    auto it = std::find_if(regions.begin(), regions.end(), [name](const std::unique_ptr<Region>& it) { return it->id() == name; });
    if (it == regions.end()) {
        return nullptr;
    }
    return it->get();
}

Sector* Model::find_sector(const std::string& name) const {
    auto it = std::find_if(sectors.begin(), sectors.end(), [name](const std::unique_ptr<Sector>& it) { return it->id() == name; });
    if (it == sectors.end()) {
        return nullptr;
    }
    return it->get();
}

Firm* Model::find_firm(const std::string& sector_name, const std::string& region_name) const {
    Sector* sector = find_sector(sector_name);
    if (sector) {
        return find_firm(sector, region_name);
    }
    return nullptr;
}

Firm* Model::find_firm(Sector* sector, const std::string& region_name) const {
    auto it = std::find_if(sector->firms.begin(), sector->firms.end(), [region_name](const Firm* it) { return it->region->id() == region_name; });
    if (it == sector->firms.end()) {
        return nullptr;
    }
    return *it;
}

Consumer* Model::find_consumer(Region* region) const {
    auto it = std::find_if(region->economic_agents.begin(), region->economic_agents.end(),
                           [](const std::unique_ptr<EconomicAgent>& it) { return it->type == EconomicAgent::Type::CONSUMER; });
    if (it == region->economic_agents.end()) {
        return nullptr;
    }
    return it->get()->as_consumer();
}

Consumer* Model::find_consumer(const std::string& region_name) const {
    Region* region = find_region(region_name);
    if (region) {
        return find_consumer(region);
    }
    return nullptr;
}

GeoLocation* Model::find_location(const std::string& name) const {
    auto it =
        std::find_if(other_locations.begin(), other_locations.end(), [name](const std::unique_ptr<GeoLocation>& it) { return it->id() == name; });
    if (it == other_locations.end()) {
        return nullptr;
    }
    return it->get();
}

void Model::switch_registers() {
    assertstep(SCENARIO);
    current_register_ = 1 - current_register_;
}

void Model::tick() {
    assertstep(SCENARIO);
    time_ += delta_t_;
    ++timestep_;
}

void Model::delta_t(const Time &delta_t_p) {
    assertstep(INITIALIZATION);
    delta_t_ = delta_t_p;
}

void Model::start_time(const Time &start_time) {
    assertstep(INITIALIZATION);
    start_time_ = start_time;
}

void Model::stop_time(const Time &stop_time) {
    assertstep(INITIALIZATION);
    stop_time_ = stop_time;
}

void Model::no_self_supply(bool no_self_supply_p) {
    assertstep(INITIALIZATION);
    no_self_supply_ = no_self_supply_p;
}

typename VariantPrices::ModelParameters &Model::parameters_writable() {
    assertstep(INITIALIZATION);
    return parameters_;
}

}  // namespace acclimate
