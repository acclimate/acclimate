// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/GeoRoute.h"

#include "model/GeoEntity.h"

namespace acclimate {

auto GeoRoute::name() const -> std::string {
    std::string res;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i > 0) {
            res += "->";
        }
        res += path[i]->name();
    }
    return res;
}

}  // namespace acclimate
