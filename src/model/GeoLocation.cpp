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
#include <memory>
#include <utility>

#include "ModelRun.h"
#include "acclimate.h"
#include "model/GeoConnection.h"
#include "model/GeoPoint.h"
#include "model/Model.h"

namespace acclimate {

GeoLocation::GeoLocation(Model* const model_m, TransportDelay delay_p, GeoLocation::Type type_p, std::string id_p)
    : GeoEntity(model_m, delay_p, GeoEntity::Type::LOCATION), type(type_p), id_m(std::move(id_p)) {}

void GeoLocation::remove_connection(const GeoConnection* connection) {
    auto it = std::find_if(std::begin(connections), std::end(connections), [connection](const auto& c) { return c.get() == connection; });
    if (it == std::end(connections)) {
        throw log::error(this, "Connection ", connection->id(), " not found");
    }
    connections.erase(it);
}

GeoLocation::~GeoLocation() {
    for (auto& connection : connections) {
        connection->invalidate_location(this);
    }
}

void GeoLocation::set_centroid(std::unique_ptr<GeoPoint>& centroid_p) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    centroid_m = std::move(centroid_p);
}

}  // namespace acclimate
