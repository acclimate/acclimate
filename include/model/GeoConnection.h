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

#ifndef ACCLIMATE_GEOCONNECTION_H
#define ACCLIMATE_GEOCONNECTION_H

#include "model/GeoEntity.h"

namespace acclimate {

class GeoLocation;

class GeoConnection : public GeoEntity {
  public:
    enum class Type { ROAD, AVIATION, SEAROUTE, UNSPECIFIED };

  public:
    const GeoLocation* location1;  // TODO encapsulate
    const GeoLocation* location2;  // TODO encapsulate
    const Type type;

  public:
    GeoConnection(Model* model_m, TransportDelay delay, Type type_p, const GeoLocation* location1_p, const GeoLocation* location2_p);
    void invalidate_location(const GeoLocation* location);
    std::string id() const override;
};
}  // namespace acclimate

#endif
