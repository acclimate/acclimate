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

#include "model/GeoPoint.h"

#include <cmath>

namespace acclimate {

GeoPoint::GeoPoint(FloatType lon_p, FloatType lat_p) : lon_(lon_p), lat_(lat_p) {}

FloatType GeoPoint::distance_to(const GeoPoint& other) const {
    const auto R = 6371;
    const auto PI = 3.14159265;
    const auto latsin = std::sin((other.lat_ - lat_) * PI / 360);
    const auto lonsin = std::sin((other.lon_ - lon_) * PI / 360);

    const auto a = latsin * latsin + cos(other.lat_ * PI / 180) * cos(lat_ * PI / 180) * lonsin * lonsin;
    return 2 * R * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}
}  // namespace acclimate
