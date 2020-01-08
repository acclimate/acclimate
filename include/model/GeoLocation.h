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

#include <cassert>
#include <memory>

#include "model/GeoEntity.h"

namespace acclimate {

class GeoConnection;
class GeoPoint;
class Region;

class GeoLocation : public GeoEntity {
  public:
    enum class Type { REGION, SEA, PORT };

  protected:
    std::unique_ptr<GeoPoint> centroid_m;
    const std::string id_m;

  public:
    using GeoEntity::model;
    std::vector<std::shared_ptr<GeoConnection>> connections;
    const Type type;

    GeoLocation(Model* model_m, TransportDelay delay_p, Type type_p, std::string id_p);
    ~GeoLocation() override;

    virtual inline Region* as_region() {
        assert(type == Type::REGION);
        return nullptr;
    }

    virtual inline const Region* as_region() const {
        assert(type == Type::REGION);
        return nullptr;
    }

    void set_centroid(std::unique_ptr<GeoPoint>& centroid_p);

    const GeoPoint* centroid() const { return centroid_m.get(); }

    void remove_connection(const GeoConnection* connection);

    inline std::string id() const override { return id_m; }
};
}  // namespace acclimate

#endif
