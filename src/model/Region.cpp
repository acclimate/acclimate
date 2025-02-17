// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/Region.h"

#include "acclimate.h"
#include "model/Government.h"
#include "model/Model.h"

namespace acclimate {

Region::Region(Model* model, id_t id) : GeoLocation(model, std::move(id), 0, GeoLocation::type_t::REGION) {}

Region::~Region() = default;

void Region::add_export(const Flow& flow) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    export_flow_lock_.call([&]() { export_flow_[model()->current_register()] += flow; });
}

void Region::add_import(const Flow& flow) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    import_flow_lock_.call([&]() { import_flow_[model()->current_register()] += flow; });
}

void Region::add_consumption(const Flow& flow) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    consumption_flow_Y_lock_.call([&]() { consumption_flow_Y_[model()->current_register()] += flow; });
}

auto Region::get_gdp() const -> Flow {
    return consumption_flow_Y_[model()->current_register()] + export_flow_[model()->current_register()] - import_flow_[model()->current_register()];
}

void Region::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    export_flow_[model()->other_register()] = Flow(0.0);
    import_flow_[model()->other_register()] = Flow(0.0);
    consumption_flow_Y_[model()->other_register()] = Flow(0.0);
    if (government_) {
        government_->iterate_consumption_and_production();
    }
}

void Region::iterate_expectation() {
    debug::assertstep(this, IterationStep::EXPECTATION);
    if (government_) {
        government_->iterate_expectation();
    }
}

void Region::iterate_purchase() {
    debug::assertstep(this, IterationStep::PURCHASE);
    if (government_) {
        government_->iterate_purchase();
    }
}

void Region::iterate_investment() {
    debug::assertstep(this, IterationStep::INVESTMENT);
    if (government_) {
        government_->iterate_investment();
    }
}

auto Region::find_path_to(Region* region, Sector::transport_type_t transport_type) -> GeoRoute& {
    const auto& it = routes_.find(std::make_pair(region->id.index(), transport_type));
    if (it == std::end(routes_)) {
        throw log::error(this, "No transport data from ", name(), " to ", region->name(), " via ", Sector::unmap_transport_type(transport_type));
    }
    return it->second;
}

auto Region::consumption() const -> const Flow& {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return consumption_flow_Y_[model()->current_register()];
}

auto Region::import_flow() const -> const Flow& {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return import_flow_[model()->current_register()];
}

auto Region::export_flow() const -> const Flow& {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return export_flow_[model()->current_register()];
}

void Region::set_government(Government* government_p) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    if (government_) {
        throw log::error(this, "Government already set");
    }
    government_.reset(government_p);
}

auto Region::government() -> Government* { return government_.get(); }

auto Region::government() const -> const Government* { return government_.get(); }

}  // namespace acclimate
