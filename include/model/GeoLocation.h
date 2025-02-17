// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_GEOLOCATION_H
#define ACCLIMATE_GEOLOCATION_H

#include <memory>
#include <string>
#include <vector>

#include "acclimate.h"
#include "model/GeoEntity.h"

namespace acclimate {

class GeoConnection;
class GeoPoint;
class Model;
class Region;

class GeoLocation : public GeoEntity {
  public:
    enum class type_t { REGION, SEA, PORT };

  protected:
    std::unique_ptr<GeoPoint> centroid_;

  public:
    std::vector<std::shared_ptr<GeoConnection>> connections;
    const GeoLocation::type_t type;
    const id_t id;

  public:
    GeoLocation(Model* model, id_t id_, TransportDelay delay, GeoLocation::type_t type_);
    virtual ~GeoLocation() override;
    void set_centroid(FloatType lon, FloatType lat);
    const GeoPoint* centroid() const { return centroid_.get(); }
    void remove_connection(const GeoConnection* connection);

    GeoLocation* as_location() override { return this; }
    const GeoLocation* as_location() const override { return this; }
    virtual Region* as_region() {
        assert(type == GeoLocation::type_t::REGION);
        return nullptr;
    }
    virtual const Region* as_region() const {
        assert(type == GeoLocation::type_t::REGION);
        return nullptr;
    }

    std::string name() const override { return id.name; }
};
}  // namespace acclimate

#endif
