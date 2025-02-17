// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_GEOPOINT_H
#define ACCLIMATE_GEOPOINT_H

#include "acclimate.h"

namespace acclimate {

class GeoPoint final {
  private:
    const FloatType lon_, lat_;

  public:
    GeoPoint(FloatType lon, FloatType lat);
    FloatType distance_to(const GeoPoint& other) const;

    FloatType lon() const { return lon_; }

    FloatType lat() const { return lat_; }
};
}  // namespace acclimate

#endif
