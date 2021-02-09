/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
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

#include "model/Region.h"

#include <iterator>
#include <type_traits>
#include <utility>

#include "acclimate.h"
#include "model/Government.h"
#include "model/Model.h"

namespace acclimate {

Region::Region(Model* model_p, id_t id_p) : GeoLocation(model_p, std::move(id_p), 0, GeoLocation::type_t::REGION) {}

Region::~Region() = default;

void Region::add_export_Z(const Flow& export_flow_Z_p) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    export_flow_Z_lock.call([&]() { export_flow_Z_[model()->current_register()] += export_flow_Z_p; });
}

void Region::add_import_Z(const Flow& import_flow_Z_p) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    import_flow_Z_lock.call([&]() { import_flow_Z_[model()->current_register()] += import_flow_Z_p; });
}

void Region::add_consumption_flow_Y(const Flow& consumption_flow_Y_p) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    consumption_flow_Y_lock.call([&]() { consumption_flow_Y_[model()->current_register()] += consumption_flow_Y_p; });
}

Flow Region::get_gdp() const {
    return consumption_flow_Y_[model()->current_register()] + export_flow_Z_[model()->current_register()] - import_flow_Z_[model()->current_register()];
}

void Region::iterate_consumption_and_production() {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    export_flow_Z_[model()->other_register()] = Flow(0.0);
    import_flow_Z_[model()->other_register()] = Flow(0.0);
    consumption_flow_Y_[model()->other_register()] = Flow(0.0);
    if (government_m) {
        government_m->iterate_consumption_and_production();
    }
}

void Region::iterate_expectation() {
    debug::assertstep(this, IterationStep::EXPECTATION);
    if (government_m) {
        government_m->iterate_expectation();
    }
}

void Region::iterate_purchase() {
    debug::assertstep(this, IterationStep::PURCHASE);
    if (government_m) {
        government_m->iterate_purchase();
    }
}

void Region::iterate_investment() {
    debug::assertstep(this, IterationStep::INVESTMENT);
    if (government_m) {
        government_m->iterate_investment();
    }
}

GeoRoute& Region::find_path_to(Region* region, Sector::transport_type_t transport_type) {
    const auto& it = routes.find(std::make_pair(region->id.index(), transport_type));
    if (it == std::end(routes)) {
        throw log::error(this, "No transport data from ", name(), " to ", region->name(), " via ", Sector::unmap_transport_type(transport_type));
    }
    return it->second;
}

const Flow& Region::consumption_C() const {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return consumption_flow_Y_[model()->current_register()];
}

const Flow& Region::import_flow_Z() const {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return import_flow_Z_[model()->current_register()];
}

const Flow& Region::export_flow_Z() const {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return export_flow_Z_[model()->current_register()];
}

void Region::set_government(Government* government_p) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    if (government_m) {
        throw log::error(this, "Government already set");
    }
    government_m.reset(government_p);
}

Government* Region::government() { return government_m.get(); }

const Government* Region::government() const { return government_m.get(); }

const Parameters::RegionParameters& Region::parameters_writable() const {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    return parameters_m;
}

}  // namespace acclimate
