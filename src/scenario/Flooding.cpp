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
#include <algorithm>
#include <string>
#include "run.h"
#include "settingsnode.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Flooding<ModelVariant>::Flooding(const settings::SettingsNode& settings_p, Model<ModelVariant>* model_p)
    : RasteredScenario<ModelVariant, FloatType>(settings_p, model_p) {
    const auto& scenario_node = settings_p["scenario"];
    if (scenario_node.has("sectors")) {
        for (const auto& sector_node : scenario_node["sectors"].as_sequence()) {
            const auto& sector_name = sector_node.as<std::string>();
            const auto& sector = model_p->find_sector(sector_name);
            if (!sector) {
                error("could not find sector " << sector_name);
            }
            sectors.push_back(sector->index());
        }
    }
}

template<class ModelVariant>
FloatType Flooding<ModelVariant>::new_region_forcing(Region<ModelVariant>* region) const {
    UNUSED(region);
    return 0.0;
}

template<class ModelVariant>
void Flooding<ModelVariant>::reset_forcing(Region<ModelVariant>* region, FloatType& forcing) const {
    UNUSED(region);
    forcing = 0.0;
}

template<class ModelVariant>
void Flooding<ModelVariant>::set_region_forcing(Region<ModelVariant>* region, const FloatType& forcing, FloatType proxy_sum) const {
    for (auto& it : region->economic_agents) {
        if (it->is_firm()) {
            if (sectors.empty() || std::find(sectors.begin(), sectors.end(), it->as_firm()->sector->index()) != sectors.end()) {
                it->forcing(1.0 - forcing / proxy_sum);
            }
        }
    }
}

template<class ModelVariant>
void Flooding<ModelVariant>::add_cell_forcing(
    FloatType x, FloatType y, FloatType proxy_value, FloatType cell_forcing, const Region<ModelVariant>* region, FloatType& region_forcing) const {
    UNUSED(region);
    UNUSED(x);
    UNUSED(y);
    region_forcing += cell_forcing * proxy_value;
}

INSTANTIATE_BASIC(Flooding);
}  // namespace acclimate
