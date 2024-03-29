/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
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
    non_owning_ptr<GeoLocation> location1;
    non_owning_ptr<GeoLocation> location2;

  public:
    const GeoConnection::type_t type;

  public:
    GeoConnection(Model* model_m, TransportDelay delay, GeoConnection::type_t type_p, GeoLocation* location1_p, GeoLocation* location2_p);
    void invalidate_location(const GeoLocation* location);

    GeoConnection* as_connection() override { return this; }
    const GeoConnection* as_connection() const override { return this; }

    std::string name() const override;
};
}  // namespace acclimate

#endif
