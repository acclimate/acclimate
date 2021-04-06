/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
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
#include "model/GeoLocation.h"
#include "netcdfpp.h"
#include "settingsnode.h"

namespace acclimate {

EventSeriesScenario::EventSeriesScenario(const settings::SettingsNode& settings_p, settings::SettingsNode scenario_node_p, Model* model_p)
    : ExternalScenario(settings_p, std::move(scenario_node_p), model_p) {}

ExternalForcing* EventSeriesScenario::read_forcing_file(const std::string& filename, const std::string& variable_name) {
    return new EventForcing(filename, variable_name, model());
}

void EventSeriesScenario::read_forcings() {
    {
        auto forcing_l = static_cast<EventForcing*>(forcing.get());  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        for (std::size_t i = 0; i < forcing_l->agents.size(); ++i) {
            if (forcing_l->agents[i] != nullptr) {
                if (std::isnan(forcing_l->forcings[i])) {
                    forcing_l->agents[i]->set_forcing(1.0);
                } else {
                    forcing_l->agents[i]->set_forcing(forcing_l->forcings[i]);
                }
            }
        }
    }
    {
        auto forcing_l = static_cast<EventForcing*>(forcing.get());
        for (std::size_t i = 0; i < forcing_l->locations.size(); ++i) {
            if (forcing_l->locations[i] != nullptr) {
                if (forcing_l->forcings[i] < 0.0) {
                    forcing_l->locations[i]->set_forcing_nu(-1.);
                } else {
                    forcing_l->locations[i]->set_forcing_nu(forcing_l->forcings[i]);
                }
            }
        }
    }
}


void EventSeriesScenario::EventForcing::read_data() {
    if (agents.size() != 0) {
        variable->read<Forcing, 3>(&forcings[0],{time_index, 0, 0}, {1, sectors_count, regions_count});
    } else if (locations.size() != 0) {
        variable->read<Forcing, 3>(&forcings[0],{time_index, 0}, {1, sea_routes_count});
    }
}

EventSeriesScenario::EventForcing::EventForcing(const std::string& filename, const std::string& variable_name, Model* model)
    : ExternalForcing(filename, variable_name) {
    if (file.variable("region") && file.variable("sector")) {
        const auto regions = file.variable("region").require().get<std::string>();
        const auto sectors = file.variable("sector").require().get<std::string>();
        regions_count = regions.size();
        sectors_count = sectors.size();
        agents.reserve(regions_count * sectors_count);
        forcings.reserve(regions_count * sectors_count);
        for (const auto& sector_name : sectors) {
            const auto* sector = model->sectors.find(sector_name);
            if (sector == nullptr) {
                throw log::error(this, "Sector '", sector_name, "' not found");
            }
            for (const auto& region_name : regions) {
                const auto name = sector_name + ":" + region_name;
                auto* agent = model->economic_agents.find(name);
                if (agent == nullptr) {
                    log::warning(this, "Agent ", name, " not found");
                }
                agents.push_back(agent);
                forcings.push_back(1.0);
            }
        }
    } 
    if (file.variable("sea_route")) {
        const auto sea_routes = file.variable("sea_route").require().get<std::string>();
        sea_routes_count = sea_routes.size();
        locations.reserve(sea_routes_count);
        forcings.reserve(sea_routes_count);
        for (const auto& sea_route_name : sea_routes) {
            GeoLocation* location = model->other_locations.find(sea_route_name);
            if (location == nullptr) {
                throw log::error(this, "sea object '", sea_route_name, "' not found");
            }
            locations.push_back(location);
            forcings.push_back(-1.);
        }
    }
}

}  // namespace acclimate
