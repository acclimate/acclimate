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

#include "scenario/DirectPopulation.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
DirectPopulation<ModelVariant>::DirectPopulation(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p)
    : RasteredScenario<ModelVariant, FloatType>(settings_p, model_p) {}

template<class ModelVariant>
FloatType DirectPopulation<ModelVariant>::new_region_forcing(Region<ModelVariant>* region) const {
    UNUSED(region);
    return 0.0;
}

template<class ModelVariant>
void DirectPopulation<ModelVariant>::reset_forcing(Region<ModelVariant>* region, FloatType& forcing) const {
    UNUSED(region);
    forcing = 0.0;
}

template<class ModelVariant>
void DirectPopulation<ModelVariant>::set_region_forcing(Region<ModelVariant>* region, const FloatType& forcing, FloatType proxy_sum) const {
    for (auto& it : region->economic_agents) {
        if (it->is_firm()) {
            it->as_firm()->forcing(1.0 - forcing / proxy_sum);
        }
    }
}

template<class ModelVariant>
void DirectPopulation<ModelVariant>::add_cell_forcing(FloatType x,
                                                      FloatType y,
                                                      FloatType proxy_value,
                                                      FloatType cell_forcing,
                                                      const Region<ModelVariant>* region,
                                                      FloatType& region_forcing) const {
    UNUSED(region);
    UNUSED(x);
    UNUSED(y);
    region_forcing += std::min(cell_forcing, proxy_value);
}

INSTANTIATE_BASIC(DirectPopulation);
}  // namespace acclimate
