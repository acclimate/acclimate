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

#include "model/SalesManagerDemand.h"
#include "model/BusinessConnection.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
SalesManagerDemand<ModelVariant>::SalesManagerDemand(Firm<ModelVariant>* firm_p) : SalesManager<ModelVariant>(firm_p) {}

template<class ModelVariant>
void SalesManagerDemand<ModelVariant>::distribute(const Flow& production_X) {
    if (sum_demand_requests_D().get_quantity() <= 0.0) {
        for (const auto& bc : business_connections) {
            bc->push_flow_Z(Flow(0.0));
        }
    } else {
        for (const auto& bc : business_connections) {
            Ratio zeta = bc->last_demand_request_D() / sum_demand_requests_D();
            bc->push_flow_Z(round(production_X * zeta));
        }
    }
}

#ifdef VARIANT_DEMAND
template class SalesManagerDemand<VariantDemand>;
#endif
}  // namespace acclimate
