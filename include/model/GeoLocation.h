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

#include "acclimate.h"
#include "model/GeoEntity.h"
#include "model/GeoPoint.h"

namespace acclimate {

template<class ModelVariant>
class GeoConnection;
template<class ModelVariant>
class Region;

template<class ModelVariant>
class GeoLocation : public GeoEntity<ModelVariant> {
  public:
    enum class Type { REGION, SEA, PORT };

  protected:
    std::unique_ptr<GeoPoint> centroid_m;
    const std::string name_m;

  public:
    std::vector<std::shared_ptr<GeoConnection<ModelVariant>>> connections;
    const Type type;

    GeoLocation(TransportDelay delay_p, Type type_p, std::string name_p);
    virtual ~GeoLocation();
    virtual inline Region<ModelVariant>* as_region() {
        assert(type == Type::REGION);
        return nullptr;
    }
    virtual inline const Region<ModelVariant>* as_region() const {
        assert(type == Type::REGION);
        return nullptr;
    }
    void set_centroid(std::unique_ptr<GeoPoint>& centroid_p) {
        assertstep(INITIALIZATION);
        return centroid_m.reset(centroid_p.release());
    }
    const GeoPoint* centroid() const { return centroid_m.get(); };
    void remove_connection( const GeoConnection<ModelVariant>* connection);
    operator std::string() const { return name_m; }
};
}  // namespace acclimate

#endif
