// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_GEOCONNECTION_H
#define ACCLIMATE_GEOCONNECTION_H

#include <string>

#include "acclimate.h"
#include "model/GeoEntity.h"

namespace acclimate {
class Model;

class GeoLocation;

class GeoConnection final : public GeoEntity {
  public:
    enum class type_t { ROAD, AVIATION, SEAROUTE, UNSPECIFIED };

  private:
    non_owning_ptr<GeoLocation> location1_;
    non_owning_ptr<GeoLocation> location2_;

  public:
    const GeoConnection::type_t type;

  public:
    GeoConnection(Model* model, TransportDelay delay, GeoConnection::type_t type, GeoLocation* location1, GeoLocation* location2);
    void invalidate_location(const GeoLocation* location);

    GeoConnection* as_connection() override { return this; }
    const GeoConnection* as_connection() const override { return this; }

    std::string name() const override;
};
}  // namespace acclimate

#endif
