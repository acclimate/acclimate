// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/EconomicAgent.h"

#include "acclimate.h"
#include "model/Region.h"
#include "model/Storage.h"

namespace acclimate {

EconomicAgent::EconomicAgent(id_t id_, Region* region_, EconomicAgent::type_t type_) : id(std::move(id_)), region(region_), type(type_) {}

EconomicAgent::~EconomicAgent() = default;  // needed to use forward declares for std::unique_ptr

auto EconomicAgent::model() -> Model* { return region->model(); }
auto EconomicAgent::model() const -> const Model* { return region->model(); }

void EconomicAgent::set_forcing(const Forcing& forcing) {
    debug::assertstep(this, IterationStep::SCENARIO);
    assert(forcing >= 0.0);
    forcing_ = forcing;
}

}  // namespace acclimate
