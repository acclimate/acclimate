// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/GeoLocation.h"

#include "acclimate.h"
#include "model/GeoConnection.h"
#include "model/GeoPoint.h"

namespace acclimate {

GeoLocation::GeoLocation(Model* model, id_t id_, TransportDelay delay, GeoLocation::type_t type_)
    : GeoEntity(model, delay, GeoEntity::type_t::LOCATION), type(type_), id(std::move(id_)) {}

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

void GeoLocation::set_centroid(FloatType lon, FloatType lat) {
    debug::assertstep(this, IterationStep::INITIALIZATION);
    centroid_ = std::make_unique<GeoPoint>(lon, lat);
}

}  // namespace acclimate
