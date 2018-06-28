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

#include "model/GeoConnection.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
GeoConnection<ModelVariant>::GeoConnection(Model<ModelVariant>* const model_m,
                                           TransportDelay delay,
                                           Type type_p,
                                           const GeoLocation<ModelVariant>* location1_p,
                                           const GeoLocation<ModelVariant>* location2_p)
    : GeoEntity<ModelVariant>(model_m, delay, GeoEntity<ModelVariant>::Type::CONNECTION), type(type_p), location1(location1_p), location2(location2_p) {}

template<class ModelVariant>
void GeoConnection<ModelVariant>::invalidate_location(const GeoLocation<ModelVariant>* location) {
    if (location1 == location) {
        location1 = nullptr;
    } else if (location2 == location) {
        location2 = nullptr;
    } else {
        error("Location not part of this connection or already invalidated");
    }
}

INSTANTIATE_BASIC(GeoConnection);
}  // namespace acclimate
