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

#include <algorithm>
#include <iterator>

#include "acclimate.h"
#include "model/TransportChainLink.h"

namespace acclimate {

GeoEntity::GeoEntity(Model* model_p, TransportDelay delay_p, Type type_p) : model_m(model_p), delay(delay_p), type_m(type_p) {}

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

void GeoEntity::remove_transport_chain_link(TransportChainLink* transport_chain_link) {
    auto it = std::find_if(std::begin(transport_chain_links), std::end(transport_chain_links),
                           [transport_chain_link](const auto l) { return l == transport_chain_link; });
    if (it == std::end(transport_chain_links)) {
        throw log::error(this, "Transport chain link ", transport_chain_link->id(), " not found");
    }
    transport_chain_links.erase(it);
}

}  // namespace acclimate
