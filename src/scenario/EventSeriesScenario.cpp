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

#include <sstream>

#include "model/Firm.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
EventSeriesScenario<ModelVariant>::EventSeriesScenario(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p)
    : ExternalScenario<ModelVariant>(settings_p, model_p) {}

template<class ModelVariant>
ExternalForcing* EventSeriesScenario<ModelVariant>::read_forcing_file(const std::string& filename, const std::string& variable_name) {
    return new EventForcing(filename, variable_name);
}

template<class ModelVariant>
void EventSeriesScenario<ModelVariant>::read_forcings() {
    auto forcing_l = static_cast<EventForcing*>(forcing.get());
    // TODO apply forcing in forcings vector to firm
}

INSTANTIATE_BASIC(EventSeriesScenario);
}  // namespace acclimate
