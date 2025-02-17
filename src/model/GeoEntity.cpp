// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/GeoEntity.h"

#include "acclimate.h"
#include "model/TransportChainLink.h"

namespace acclimate {

GeoEntity::GeoEntity(Model* model, TransportDelay delay_, GeoEntity::type_t type) : model_(model), delay(delay_), entity_type(type) {}

void GeoEntity::set_forcing_nu(Forcing forcing) {
    for (auto* link : transport_chain_links) {
        link->set_forcing(forcing);
    }
}

GeoEntity::~GeoEntity() {
    for (auto* link : transport_chain_links) {
        link->unregister_geoentity();
    }
}

}  // namespace acclimate
