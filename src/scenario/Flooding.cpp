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
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Flooding<ModelVariant>::Flooding(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p)
    : RasteredScenario<ModelVariant>(settings_p, model_p) {
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
void Flooding<ModelVariant>::set_forcing(Region<ModelVariant>* region, FloatType forcing_p) const {
    for (auto& it : region->economic_agents) {
        if (it->is_firm()) {
            if (sectors.empty() || std::find(sectors.begin(), sectors.end(), it->as_firm()->sector->index()) != sectors.end())
                it->as_firm()->forcing_lambda(1.0 - forcing_p);
        }
    }
}

template<class ModelVariant>
inline FloatType Flooding<ModelVariant>::get_affected_population_per_cell(FloatType x,
                                                                          FloatType y,
                                                                          FloatType population_p,
                                                                          FloatType external_forcing) const {
    UNUSED(x);
    UNUSED(y);
    return external_forcing * population_p;
}

INSTANTIATE_BASIC(Flooding);
}  // namespace acclimate
