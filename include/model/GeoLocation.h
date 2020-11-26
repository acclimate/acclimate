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
    std::unique_ptr<GeoPoint> centroid_m;

  public:
    std::vector<std::shared_ptr<GeoConnection>> connections;
    const GeoLocation::type_t type;
    const id_t id;

  public:
    GeoLocation(Model* model_p, id_t id_p, TransportDelay delay_p, GeoLocation::type_t type_p);
    virtual ~GeoLocation() override;
    void set_centroid(FloatType lon_p, FloatType lat_p);
    const GeoPoint* centroid() const { return centroid_m.get(); }
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
