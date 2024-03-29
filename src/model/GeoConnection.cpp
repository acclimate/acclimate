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

#include "model/GeoConnection.h"

#include "acclimate.h"
#include "model/GeoLocation.h"

namespace acclimate {

class Model;

GeoConnection::GeoConnection(Model* model_m, TransportDelay delay, GeoConnection::type_t type_p, GeoLocation* location1_p, GeoLocation* location2_p)
    : GeoEntity(model_m, delay, GeoEntity::type_t::CONNECTION), type(type_p), location1(location1_p), location2(location2_p) {}

void GeoConnection::invalidate_location(const GeoLocation* location) {
    if (location1 == location) {
        location1.invalidate();
    } else if (location2 == location) {
        location2.invalidate();
    } else {
        throw log::error(this, "Location not part of this connection or already invalidated");
    }
}

std::string GeoConnection::name() const { return (location1 ? location1->name() : "INVALID") + "-" + (location2 ? location2->name() : "INVALID"); }

}  // namespace acclimate
