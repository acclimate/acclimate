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

#include "scenario/EventSeriesScenario.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "acclimate.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/Sector.h"
#include "netcdfpp.h"
#include "settingsnode.h"

namespace acclimate {

EventSeriesScenario::EventSeriesScenario(const settings::SettingsNode& settings_p, settings::SettingsNode scenario_node_p, Model* model_p)
    : ExternalScenario(settings_p, std::move(scenario_node_p), model_p) {}

ExternalForcing* EventSeriesScenario::read_forcing_file(const std::string& filename, const std::string& variable_name) {
    return new EventForcing(filename, variable_name, model());
}

void EventSeriesScenario::read_forcings() {
    auto forcing_l = static_cast<EventForcing*>(forcing.get());  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
    for (std::size_t i = 0; i < forcing_l->firms.size(); ++i) {
        if (forcing_l->firms[i] != nullptr) {
            if (std::isnan(forcing_l->forcings[i])) {
                forcing_l->firms[i]->set_forcing(1.0);
            } else {
                forcing_l->firms[i]->set_forcing(forcing_l->forcings[i]);
            }
        }
    }
}

void EventSeriesScenario::EventForcing::read_data() { variable->read<Forcing, 3>(&forcings[0], {time_index, 0, 0}, {1, sectors_count, regions_count}); }

EventSeriesScenario::EventForcing::EventForcing(const std::string& filename, const std::string& variable_name, const Model* model)
    : ExternalForcing(filename, variable_name) {
    const auto regions = file.variable("region").require().get<std::string>();
    const auto sectors = file.variable("sector").require().get<std::string>();
    regions_count = regions.size();
    sectors_count = sectors.size();
    firms.reserve(regions_count * sectors_count);
    forcings.reserve(regions_count * sectors_count);
    for (const auto& sector_name : sectors) {
        const auto* sector = model->sectors.find(sector_name);
        if (sector == nullptr) {
            throw log::error(this, "sector '", sector_name, "' not found");
        }
        for (const auto& region_name : regions) {
            Firm* firm = nullptr;  // TODO model->find_firm(sector, region_name);
            if (firm == nullptr) {
                log::warning(this, "firm '", sector_name, ":", region_name, "' not found");
            }
            firms.push_back(firm);
            forcings.push_back(1.0);
        }
    }
}

}  // namespace acclimate
