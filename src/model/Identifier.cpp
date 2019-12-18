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

#include "model/Sector.h"
#include <utility>
#include "variants/ModelVariants.h"

namespace acclimate {

// Identifier to store name of economic agent and id, could be extended to individual properties
template<class ModelVariant>
Identifier<ModelVariant>::Identifier(Model<ModelVariant>* model_p,
                             std::string id_p,
                             const IntType index_p)
    :model_m(model_p),
     id_m(std::move(id_p)),
     index_m(index_p)
      {}

    template<class ModelVariant>
    void Sector<ModelVariant>::remove_firm(Firm<ModelVariant>* firm) {
        for (auto it = firms.begin(); it != firms.end(); ++it) {  // TODO use find_if
            if (*it == firm) {
                firms.erase(it);
                break;
            }
        }
    }

      INSTANTIATE_BASIC(Identifier);
}  // namespace acclimate
