// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/Sector.h"

#include "acclimate.h"

namespace acclimate {

Sector::Sector(Model* model, id_t id_, Ratio upper_storage_limit_, Time baseline_storage_fill_factor_, transport_type_t transport_type_)
    : id(std::move(id_)),
      model_(model),
      upper_storage_limit(upper_storage_limit_),
      baseline_storage_fill_factor(std::move(baseline_storage_fill_factor_)),
      transport_type(transport_type_) {}

void Sector::add_demand_request(const Demand& demand_request) {
    debug::assertstep(this, IterationStep::PURCHASE);
    total_demand_lock_.call([&]() { total_demand_ += demand_request; });
}

void Sector::add_production(const Flow& flow) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    total_production_lock_.call([&]() { total_production_ += flow; });
}

void Sector::add_baseline_production(const Flow& flow) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    last_total_production_ += flow;
    total_production_ += flow;
}

void Sector::subtract_baseline_production(const Flow& flow) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    last_total_production_ -= flow;
    total_production_ -= flow;
}

void Sector::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    total_demand_ = Demand(0.0);
    last_total_production_ = total_production_;
    total_production_ = Flow(0.0);
}

auto Sector::map_transport_type(const hashed_string& transport_type) -> Sector::transport_type_t {
    switch (transport_type) {
        case hash("aviation"):
            return transport_type_t::AVIATION;
        case hash("immediate"):
            return transport_type_t::IMMEDIATE;
        case hash("roadsea"):
            return transport_type_t::ROADSEA;
        default:
            throw log::error("Unknown transport type ", transport_type);
    }
}

auto Sector::unmap_transport_type(Sector::transport_type_t transport_type) -> const char* {
    switch (transport_type) {
        case transport_type_t::AVIATION:
            return "aviation";
        case transport_type_t::IMMEDIATE:
            return "immediate";
        case transport_type_t::ROADSEA:
            return "roadsea";
        default:
            throw log::error("Unkown transport type");
    }
}

auto Sector::total_demand() const -> const Demand& {
    debug::assertstepnot(this, IterationStep::PURCHASE);
    return total_demand_;
}

auto Sector::total_production() const -> const Demand& {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return total_production_;
}

}  // namespace acclimate
