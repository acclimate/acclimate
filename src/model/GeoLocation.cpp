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

#include "model/GeoLocation.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "acclimate.h"
#include "model/GeoConnection.h"
#include "model/GeoPoint.h"

namespace acclimate {

GeoLocation::GeoLocation(Model* model_p, id_t id_p, TransportDelay delay_p, GeoLocation::type_t type_p)
    : GeoEntity(model_p, delay_p, GeoEntity::type_t::LOCATION), type(type_p), id(std::move(id_p)) {}

void GeoLocation::remove_connection(const GeoConnection* connection) {
    auto it = std::find_if(std::begin(connections), std::end(connections), [connection](const auto& c) { return c.get() == connection; });
    if (it == std::end(connections)) {
        throw log::error(this, "Connection ", connection->name(), " not found");
    }
    connections.erase(it);
}

GeoLocation::~GeoLocation() {
    for (auto& connection : connections) {
        connection->invalidate_location(this);
    }
}

void GeoLocation::set_centroid(FloatType lon_p, FloatType lat_p) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    centroid_m = std::make_unique<GeoPoint>(lon_p, lat_p);
}

}  // namespace acclimate
