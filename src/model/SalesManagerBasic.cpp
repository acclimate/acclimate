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

#include "model/SalesManagerBasic.h"
#include "model/BusinessConnection.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
SalesManagerBasic<ModelVariant>::SalesManagerBasic(Firm<ModelVariant>* firm_p) : SalesManager<ModelVariant>(firm_p) {}

template<class ModelVariant>
void SalesManagerBasic<ModelVariant>::distribute(const Flow& production_X) {
    Flow initial_production_X = firm->initial_production_X_star();

    // Calculates output distribution key zeta without prioritizing any buyer
    for (auto it = business_connections.begin(); it != business_connections.end(); ++it) {
        (*it)->push_flow_Z(round(production_X * ((*it)->initial_flow_Z_star() / initial_production_X)));
    }
}

#ifdef VARIANT_BASIC
template class SalesManagerBasic<VariantBasic>;
#endif
}  // namespace acclimate
