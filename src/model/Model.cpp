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
#include "model/Consumer.h"
#include "model/Firm.h"
#include "model/Infrastructure.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "output/Output.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Model<ModelVariant>::Model() : consumption_sector(new Sector<ModelVariant>(this, "FCON", 0, Ratio(0.0), Time(0.0))) {
    sectors_C.emplace_back(consumption_sector);
}

template<class ModelVariant>
void Model<ModelVariant>::start(const Time& start_time) {
    time_ = start_time;
    timestep_ = 0;
}

template<class ModelVariant>
void Model<ModelVariant>::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < sectors_C.size(); ++i) {
        sectors_C[i]->iterate_consumption_and_production();
    }

#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < regions_R.size(); ++i) {
        regions_R[i]->iterate_consumption_and_production();
    }
}

template<class ModelVariant>
void Model<ModelVariant>::iterate_expectation() {
    assertstep(EXPECTATION);
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < regions_R.size(); ++i) {
        regions_R[i]->iterate_expectation();
    }
}

template<class ModelVariant>
void Model<ModelVariant>::iterate_purchase() {
    assertstep(PURCHASE);
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < regions_R.size(); ++i) {
        regions_R[i]->iterate_purchase();
    }
}

template<class ModelVariant>
void Model<ModelVariant>::iterate_investment() {
    assertstep(INVESTMENT);
#pragma omp parallel for default(shared) schedule(guided)
    for (std::size_t i = 0; i < regions_R.size(); ++i) {
        regions_R[i]->iterate_investment();
    }
}

template<class ModelVariant>
Region<ModelVariant>* Model<ModelVariant>::find_region(const std::string& name) const {
    auto it = std::find_if(regions_R.begin(), regions_R.end(), [name](const std::unique_ptr<Region<ModelVariant>>& it) { return it->id() == name; });
    if (it == regions_R.end()) {
        return nullptr;
    }
    return it->get();
}

template<class ModelVariant>
Sector<ModelVariant>* Model<ModelVariant>::find_sector(const std::string& name) const {
    auto it = std::find_if(sectors_C.begin(), sectors_C.end(), [name](const std::unique_ptr<Sector<ModelVariant>>& it) { return it->id() == name; });
    if (it == sectors_C.end()) {
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
Firm<ModelVariant>* Model<ModelVariant>::find_firm(Sector<ModelVariant>* sector, const std::string& region_name) const {
    auto it =
        std::find_if(sector->firms_N.begin(), sector->firms_N.end(), [region_name](const Firm<ModelVariant>* it) { return it->region->id() == region_name; });
    if (it == sector->firms_N.end()) {
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

INSTANTIATE_BASIC(Model);
}  // namespace acclimate
