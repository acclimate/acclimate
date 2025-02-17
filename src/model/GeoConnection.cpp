// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/GeoConnection.h"

#include "acclimate.h"
#include "model/GeoLocation.h"

namespace acclimate {

class Model;

GeoConnection::GeoConnection(Model* model, TransportDelay delay, GeoConnection::type_t type, GeoLocation* location1, GeoLocation* location2)
    : GeoEntity(model, delay, GeoEntity::type_t::CONNECTION), type(type), location1_(location1), location2_(location2) {}

void GeoConnection::invalidate_location(const GeoLocation* location) {
    if (location1_ == location) {
        location1_.invalidate();
    } else if (location2_ == location) {
        location2_.invalidate();
    } else {
        throw log::error(this, "Location not part of this connection or already invalidated");
    }
}

auto GeoConnection::name() const -> std::string {
    return (location1_ != nullptr ? location1_->name() : "INVALID") + "-" + (location2_ != nullptr ? location2_->name() : "INVALID");
}

}  // namespace acclimate
