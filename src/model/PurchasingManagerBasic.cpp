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

#include "model/PurchasingManagerBasic.h"
#include "model/BusinessConnection.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
PurchasingManagerBasic<ModelVariant>::PurchasingManagerBasic(Storage<ModelVariant>* storage_p) : PurchasingManager<ModelVariant>(storage_p) {}

template<class ModelVariant>
void PurchasingManagerBasic<ModelVariant>::iterate_purchase() {
    for (auto it = business_connections.begin(); it != business_connections.end(); it++) {
        (*it)->send_demand_request_D((*it)->initial_flow_Z_star());
    }
}

template class PurchasingManagerBasic<VariantBasic>;
}  // namespace acclimate
