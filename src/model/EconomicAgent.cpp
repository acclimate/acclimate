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

#include "model/EconomicAgent.h"

#include <algorithm>
#include <iterator>

#include "acclimate.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "model/Storage.h"

namespace acclimate {

EconomicAgent::EconomicAgent(std::string name_p, Sector* sector_p, Region* region_p, const EconomicAgent::Type& type_p)
    : name(std::move(name_p)), sector(sector_p), region(region_p), type(type_p) {}

inline Firm* EconomicAgent::as_firm() {
    assert(type == Type::FIRM);
    return nullptr;
}

inline Consumer* EconomicAgent::as_consumer() {
    assert(type == Type::CONSUMER);
    return nullptr;
}

inline const Firm* EconomicAgent::as_firm() const {
    assert(type == Type::FIRM);
    return nullptr;
}

inline const Consumer* EconomicAgent::as_consumer() const {
    assert(type == Type::CONSUMER);
    return nullptr;
}

Storage* EconomicAgent::find_input_storage(const std::string& sector_name) const {
    auto it = std::find_if(std::begin(input_storages), std::end(input_storages), [sector_name](const auto& is) { return is->sector->id() == sector_name; });
    if (it == std::end(input_storages)) {
        return nullptr;
    }
    return it->get();
}

void EconomicAgent::remove_storage(Storage* storage) {
    auto it = std::find_if(std::begin(input_storages), std::end(input_storages), [storage](const auto& is) { return is.get() == storage; });
    input_storages.erase(it);
}

Model* EconomicAgent::model() const { return sector->model(); }

Parameters::AgentParameters const& EconomicAgent::parameters_writable() const {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    return parameters_;
}

void EconomicAgent::set_forcing(const Forcing& forcing_p) {
    debug::assertstep(this, IterationStep::SCENARIO);
    assert(forcing_p >= 0.0);
    forcing_ = forcing_p;
}

}  // namespace acclimate
