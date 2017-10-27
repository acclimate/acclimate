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

#ifndef ACCLIMATE_EVENTSERIESSCENARIO_H
#define ACCLIMATE_EVENTSERIESSCENARIO_H

#include "scenario/ExternalForcing.h"
#include "scenario/ExternalScenario.h"

namespace acclimate {

template<class ModelVariant>
class Region;
template<class ModelVariant>
class Firm;

template<class ModelVariant>
class EventSeriesScenario : public ExternalScenario<ModelVariant> {
  protected:
    using ExternalScenario<ModelVariant>::forcing;
    using ExternalScenario<ModelVariant>::model;

    class EventForcing : public ExternalForcing {
        friend class EventSeriesScenario<ModelVariant>;

      protected:
        using ExternalForcing::file;
        using ExternalForcing::time_index;
        using ExternalForcing::variable;
        std::vector<Firm<ModelVariant>*> firms;
        std::vector<Forcing> forcings;
        std::size_t regions_count;
        std::size_t sectors_count;

        void read_data() override;

      public:
        EventForcing(const std::string& filename, const std::string& variable_name, const Model<ModelVariant>* model);
    };

    ExternalForcing* read_forcing_file(const std::string& filename, const std::string& variable_name) override;
    void read_forcings() override;

  public:
    EventSeriesScenario(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p);
};
}  // namespace acclimate

#endif
