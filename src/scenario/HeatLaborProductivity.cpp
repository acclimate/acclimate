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

#include "scenario/HeatLaborProductivity.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include "model/EconomicAgent.h"
#include "settingsnode.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
HeatLaborProductivity<ModelVariant>::HeatLaborProductivity(const settings::SettingsNode& settings_p,
                                                           settings::SettingsNode scenario_node_p,
                                                           Model<ModelVariant>* const model_p)
    : RasteredScenario<ModelVariant, HeatLaborProductivity<ModelVariant>::RegionForcingType>(settings_p, scenario_node_p, model_p) {}

template<class ModelVariant>
typename HeatLaborProductivity<ModelVariant>::RegionForcingType HeatLaborProductivity<ModelVariant>::new_region_forcing(Region<ModelVariant>* region) const {
    if (region) {
        return std::vector<FloatType>(region->economic_agents.size(), 0.0);
    }
    return std::vector<FloatType>();
}

template<class ModelVariant>
void HeatLaborProductivity<ModelVariant>::reset_forcing(Region<ModelVariant>* region,
                                                        typename HeatLaborProductivity<ModelVariant>::RegionForcingType& forcing) const {
    for (std::size_t i = 0; i < region->economic_agents.size(); ++i) {
        forcing[i] = 0.0;
    }
}

template<class ModelVariant>
void HeatLaborProductivity<ModelVariant>::set_region_forcing(Region<ModelVariant>* region,
                                                             const HeatLaborProductivity<ModelVariant>::RegionForcingType& forcing,
                                                             FloatType proxy_sum) const {
    for (std::size_t i = 0; i < region->economic_agents.size(); ++i) {
        auto& it = region->economic_agents[i];
        if (it->type == EconomicAgent<ModelVariant>::Type::FIRM) {
            it->forcing(1 - forcing[i] / proxy_sum);
        }
    }
}

template<class ModelVariant>
void HeatLaborProductivity<ModelVariant>::add_cell_forcing(FloatType x,
                                                           FloatType y,
                                                           FloatType proxy_value,
                                                           FloatType cell_forcing,
                                                           const Region<ModelVariant>* region,
                                                           typename HeatLaborProductivity<ModelVariant>::RegionForcingType& region_forcing) const {
    UNUSED(x);
    UNUSED(y);
    for (std::size_t i = 0; i < region->economic_agents.size(); ++i) {
        const std::string& sector_name = region->economic_agents[i]->sector->id();
        FloatType alpha;
        if (cell_forcing > 300.14) {
            switch (settings::hstring::hash(sector_name.c_str())) {
                case settings::hstring::hash("AGRI"):
                    alpha = 0.008;
                    break;
                case settings::hstring::hash("FISH"):
                    alpha = 0.008;
                    break;
                case settings::hstring::hash("MINQ"):
                    alpha = 0.042;
                    break;
                case settings::hstring::hash("GAST"):
                    alpha = 0.061;
                    break;
                case settings::hstring::hash("WHOT"):
                    alpha = 0.061;
                    break;
                case settings::hstring::hash("OTHE"):
                    alpha = 0.022;
                    break;
                default:
                    alpha = 0.0;
                    break;
            }
            region_forcing[i] += std::min(1.0, alpha * (cell_forcing - 300.15)) * proxy_value;
        }
    }
}

INSTANTIATE_BASIC(HeatLaborProductivity);
}  // namespace acclimate
