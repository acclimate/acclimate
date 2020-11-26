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

#include <utility>

#include "acclimate.h"
#include "model/Region.h"
#include "model/Storage.h"

namespace acclimate {

EconomicAgent::EconomicAgent(id_t id_p, Region* region_p, EconomicAgent::type_t type_p) : id(std::move(id_p)), region(region_p), type(type_p) {}

EconomicAgent::~EconomicAgent() = default;  // needed to use forward declares for std::unique_ptr

Model* EconomicAgent::model() { return region->model(); }
const Model* EconomicAgent::model() const { return region->model(); }

Parameters::AgentParameters const& EconomicAgent::parameters_writable() const {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    return parameters_;
}

void EconomicAgent::set_forcing(const Forcing& forcing_p) {
    debug::assertstep(this, IterationStep::SCENARIO);
    assert(forcing_p >= 0.0);
    forcing_m = forcing_p;
}

}  // namespace acclimate
