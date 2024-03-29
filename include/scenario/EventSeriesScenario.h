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

#ifndef ACCLIMATE_EVENTSERIESSCENARIO_H
#define ACCLIMATE_EVENTSERIESSCENARIO_H

#include <cstddef>
#include <string>
#include <vector>

#include "acclimate.h"
#include "scenario/ExternalForcing.h"
#include "scenario/ExternalScenario.h"

namespace settings {
class SettingsNode;
}  // namespace settings

namespace acclimate {

class EconomicAgent;
class Model;

class EventSeriesScenario : public ExternalScenario {
  private:
    class EventForcing : public ExternalForcing {
        friend class EventSeriesScenario;

      private:
        std::vector<EconomicAgent*> agents;  // TODO remove
        std::vector<GeoLocation*> locations;
        std::vector<Forcing> forcings;
        std::size_t regions_count;
        std::size_t sectors_count;
        std::size_t sea_routes_count;

      private:
        void read_data() override;

      public:
        EventForcing(const std::string& filename, const std::string& variable_name, Model* model);
    };

  private:
    ExternalForcing* read_forcing_file(const std::string& filename, const std::string& variable_name) override;
    void read_forcings() override;

  public:
    EventSeriesScenario(const settings::SettingsNode& settings_p, settings::SettingsNode scenario_node_p, Model* model_p);
};
}  // namespace acclimate

#endif
