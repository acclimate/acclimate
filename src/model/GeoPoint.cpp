// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/GeoPoint.h"

namespace acclimate {

GeoPoint::GeoPoint(FloatType lon, FloatType lat) : lon_(lon), lat_(lat) {}

auto GeoPoint::distance_to(const GeoPoint& other) const -> FloatType {
    const auto R = 6371;
    const auto PI = 3.14159265;
    const auto latsin = std::sin((other.lat_ - lat_) * PI / 360);
    const auto lonsin = std::sin((other.lon_ - lon_) * PI / 360);

    const auto a = latsin * latsin + cos(other.lat_ * PI / 180) * cos(lat_ * PI / 180) * lonsin * lonsin;
    return 2 * R * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}
}  // namespace acclimate
