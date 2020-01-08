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

#ifndef ACCLIMATE_GEOPOINT_H
#define ACCLIMATE_GEOPOINT_H

#include <string>

#include "types.h"

namespace acclimate {

class GeoPoint {
  private:
    const FloatType lon_, lat_;

  public:
    GeoPoint(FloatType lon_p, FloatType lat_p);
    FloatType distance_to(const GeoPoint& other) const;

    inline FloatType lon() const { return lon_; }

    inline FloatType lat() const { return lat_; }
};
}  // namespace acclimate

#endif
