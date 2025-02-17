// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_GEOROUTE_H
#define ACCLIMATE_GEOROUTE_H

#include <string>

#include "acclimate.h"

namespace acclimate {

class GeoEntity;

class GeoRoute final {
  public:
    non_owning_vector<GeoEntity> path;
    std::string name() const;
};
}  // namespace acclimate

#endif
