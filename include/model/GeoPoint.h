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

#ifndef ACCLIMATE_GEOPOINT_H
#define ACCLIMATE_GEOPOINT_H

#include "acclimate.h"

namespace acclimate {

class GeoPoint final {
  private:
    const FloatType lon_m, lat_m;

  public:
    GeoPoint(FloatType lon_p, FloatType lat_p);
    FloatType distance_to(const GeoPoint& other) const;

    FloatType lon() const { return lon_m; }

    FloatType lat() const { return lat_m; }
};
}  // namespace acclimate

#endif
