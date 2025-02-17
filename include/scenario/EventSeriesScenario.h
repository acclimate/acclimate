// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

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
        std::vector<EconomicAgent*> agents_;  // TODO remove
        std::vector<GeoLocation*> locations_;
        std::vector<Forcing> forcings_;
        std::size_t regions_count_;
        std::size_t sectors_count_;
        std::size_t sea_routes_count_;

      private:
        void read_data() override;

      public:
        EventForcing(const std::string& filename, const std::string& variable_name, Model* model);
    };

  private:
    ExternalForcing* read_forcing_file(const std::string& filename, const std::string& variable_name) override;
    void read_forcings() override;

  public:
    EventSeriesScenario(const settings::SettingsNode& settings, settings::SettingsNode scenario_node, Model* model);
};
}  // namespace acclimate

#endif
