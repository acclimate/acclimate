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

#include "scenario/Flooding.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Flooding<ModelVariant>::Flooding(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p)
    : RasteredScenario<ModelVariant, FloatType>(settings_p, model_p) {}

template<class ModelVariant>
inline FloatType Flooding<ModelVariant>::new_region_forcing(Region<ModelVariant>* region) const {
    return 0.0;
}

template<class ModelVariant>
inline void Flooding<ModelVariant>::set_region_forcing(Region<ModelVariant>* region, FloatType& forcing, const FloatType& proxy_sum) const {
    for (auto& it : region->economic_agents) {
        if (it->type == EconomicAgent<ModelVariant>::Type::FIRM) {
            it->forcing(1.0 - forcing / proxy_sum);
            forcing = 0.0;
        }
    }
}

template<class ModelVariant>
inline FloatType Flooding<ModelVariant>::add_cell_forcing(const FloatType& x,
                                                          const FloatType& y,
                                                          const FloatType& proxy_value,
                                                          const FloatType& cell_forcing,
                                                          const Region<ModelVariant>* region,
                                                          FloatType& region_forcing) const {
    UNUSED(x);
    UNUSED(y);
    region_forcing = cell_forcing * proxy_value;
    return region_forcing;
}

INSTANTIATE_BASIC(Flooding);
}  // namespace acclimate
