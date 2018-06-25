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

#include "model/GeographicEntity.h"
#include <algorithm>
#include "run.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
GeographicEntity<ModelVariant>::GeographicEntity(const GeographicEntity<ModelVariant>::Type& type_p) : type(type_p) {}

template<class ModelVariant>
GeographicEntity<ModelVariant>::~GeographicEntity() {
    for (auto it = connections.begin(); it != connections.end(); ++it) {
        (*it)->remove_connection(this);
    }
}

template<class ModelVariant>
void GeographicEntity<ModelVariant>::remove_connection(const GeographicEntity* geographic_entity) {
    auto it =
        std::find_if(connections.begin(), connections.end(), [geographic_entity](const GeographicEntity<ModelVariant>* it) { return it == geographic_entity; });
    connections.erase(it);
}

template<class ModelVariant>
inline Region<ModelVariant>* GeographicEntity<ModelVariant>::as_region() {
    assert(type == Type::REGION);
    return nullptr;
}

template<class ModelVariant>
inline Infrastructure<ModelVariant>* GeographicEntity<ModelVariant>::as_infrastructure() {
    assert(type == Type::INFRASTRUCTURE);
    return nullptr;
}

template<class ModelVariant>
inline const Region<ModelVariant>* GeographicEntity<ModelVariant>::as_region() const {
    assert(type == Type::REGION);
    return nullptr;
}

template<class ModelVariant>
inline const Infrastructure<ModelVariant>* GeographicEntity<ModelVariant>::as_infrastructure() const {
    assert(type == Type::INFRASTRUCTURE);
    return nullptr;
}

INSTANTIATE_BASIC(GeographicEntity);
}  // namespace acclimate
