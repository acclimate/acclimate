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

#include "acclimate.h"
#include "model/GeoEntity.h"

namespace acclimate {

template<class ModelVariant>
class GeoLocation;

template<class ModelVariant>
class GeoConnection : public GeoEntity<ModelVariant> {
  public:
    enum class Type { ROAD, AVIATION, SEAROUTE, UNSPECIFIED };

  public:
    const GeoLocation<ModelVariant>* location1;  // TODO encapsulate
    const GeoLocation<ModelVariant>* location2;  // TODO encapsulate
    const Type type;
    GeoConnection<ModelVariant>(Model<ModelVariant>* model_m,
                                TransportDelay delay,
                                Type type_p,
                                const GeoLocation<ModelVariant>* location1_p,
                                const GeoLocation<ModelVariant>* location2_p);
    void invalidate_location(const GeoLocation<ModelVariant>* location);
    std::string id() const override { return (location1 ? location1->id() : "INVALID") + "-" + (location2 ? location2->id() : "INVALID"); }
};
}  // namespace acclimate

#endif
