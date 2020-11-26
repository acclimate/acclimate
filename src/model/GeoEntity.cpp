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

#include "model/GeoEntity.h"

#include "acclimate.h"
#include "model/TransportChainLink.h"

namespace acclimate {

GeoEntity::GeoEntity(Model* model_p, TransportDelay delay_p, GeoEntity::type_t type_p) : model_m(model_p), delay(delay_p), entity_type(type_p) {}

void GeoEntity::set_forcing_nu(Forcing forcing_nu_p) {
    for (auto link : transport_chain_links) {
        link->set_forcing_nu(forcing_nu_p);
    }
}

GeoEntity::~GeoEntity() {
    for (auto link : transport_chain_links) {
        link->unregister_geoentity();
    }
}

}  // namespace acclimate
