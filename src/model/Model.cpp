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
#include "model/EconomicAgent.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Model<ModelVariant>::Model(Run<ModelVariant>* const run_p)
    : run_m(run_p), consumption_sector(new Sector<ModelVariant>(this, "FCON", 0, Ratio(0.0), Time(0.0), Sector<ModelVariant>::TransportType::IMMEDIATE)), consumption_identifier(new Identifier<ModelVariant>(this, "FCON",0)),void_identifier(new Identifier<ModelVariant>(this, "VOID",-1)) {
    sectors.emplace_back(consumption_sector);
}

template<class ModelVariant>
Region<ModelVariant>* Model<ModelVariant>::add_region(std::string name) {
    auto region = new Region<ModelVariant>(this, name, regions.size());
    regions.emplace_back(region);
    return region;
}

template<class ModelVariant>
Sector<ModelVariant>* Model<ModelVariant>::add_sector(std::string name,
                                                      const Ratio& upper_storage_limit_omega_p,
                                                      const Time& initial_storage_fill_factor_psi_p,
                                                      typename Sector<ModelVariant>::TransportType transport_type_p) {
    auto sector = new Sector<ModelVariant>(this, name, sectors.size(), upper_storage_limit_omega_p, initial_storage_fill_factor_psi_p, transport_type_p);
    sectors.emplace_back(sector);
    return sector;
}

template<class ModelVariant>
void Model<ModelVariant>::start() {
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

template<class ModelVariant>
void Model<ModelVariant>::iterate_consumption_and_production() {
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

template<class ModelVariant>
void Model<ModelVariant>::iterate_expectation() {
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

template<class ModelVariant>
void Model<ModelVariant>::iterate_purchase() {
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
              [](const std::pair<PurchasingManager<ModelVariant>*, std::size_t>& a, const std::pair<PurchasingManager<ModelVariant>*, std::size_t>& b) {
                  return b.second > a.second;
              });
#endif
}

template<class ModelVariant>
void Model<ModelVariant>::iterate_investment() {
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

template<class ModelVariant>
Region<ModelVariant>* Model<ModelVariant>::find_region(const std::string& name) const {
    auto it = std::find_if(regions.begin(), regions.end(), [name](const std::unique_ptr<Region<ModelVariant>>& it) { return it->id() == name; });
    if (it == regions.end()) {
        return nullptr;
    }
    return it->get();
}

template<class ModelVariant>
Sector<ModelVariant>* Model<ModelVariant>::find_sector(const std::string& name) const {
    auto it = std::find_if(sectors.begin(), sectors.end(), [name](const std::unique_ptr<Sector<ModelVariant>>& it) { return it->id() == name; });
    if (it == sectors.end()) {
        return nullptr;
    }
    return it->get();
}

template<class ModelVariant>
Firm<ModelVariant>* Model<ModelVariant>::find_firm(const std::string& sector_name, const std::string& region_name) const {
    Sector<ModelVariant>* sector = find_sector(sector_name);
    if (sector) {
        return find_firm(sector, region_name);
    }
    return nullptr;
}

template<class ModelVariant>
    Firm<ModelVariant> *Model<ModelVariant>::find_firm(Sector<ModelVariant> *sector, const std::string& region_name) const {
    auto it = std::find_if(sector->firms.begin(), sector->firms.end(), [region_name](const Firm<ModelVariant>* it) { return it->region->id() == region_name; });
    if (it == sector->firms.end()) {
        return nullptr;
    }
    return *it;
    }
template<class ModelVariant>
Firm<ModelVariant>* Model<ModelVariant>::find_firm(Identifier<ModelVariant>* identifier, Sector<ModelVariant>* sector, const std::string& identifier_name) const {
    auto it = std::find_if(sector->firms.begin(), sector->firms.end(), [identifier_name](const Firm<ModelVariant>* it) { return it->identifier->id() == identifier_name; });
    if (it == sector->firms.end()) {
        return nullptr;
    }
    return *it;
}



template<class ModelVariant>
Consumer<ModelVariant>* Model<ModelVariant>::find_consumer(Region<ModelVariant>* region) const {
    auto it = std::find_if(region->economic_agents.begin(), region->economic_agents.end(),
                           [](const std::unique_ptr<EconomicAgent<ModelVariant>>& it) { return it->type == EconomicAgent<ModelVariant>::Type::CONSUMER; });
    if (it == region->economic_agents.end()) {
        return nullptr;
    }
    return it->get()->as_consumer();
}

template<class ModelVariant>
Consumer<ModelVariant>* Model<ModelVariant>::find_consumer(const std::string& region_name) const {
    Region<ModelVariant>* region = find_region(region_name);
    if (region) {
        return find_consumer(region);
    }
    return nullptr;
}

template<class ModelVariant>
GeoLocation<ModelVariant>* Model<ModelVariant>::find_location(const std::string& name) const {
    auto it =
        std::find_if(other_locations.begin(), other_locations.end(), [name](const std::unique_ptr<GeoLocation<ModelVariant>>& it) { return it->id() == name; });
    if (it == other_locations.end()) {
        return nullptr;
    }
    return it->get();
}



    INSTANTIATE_BASIC(Model);
}  // namespace acclimate
