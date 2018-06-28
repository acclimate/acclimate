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

#ifndef ACCLIMATE_TAXES_H
#define ACCLIMATE_TAXES_H

#include "scenario/Scenario.h"
#include "types.h"

namespace acclimate {

template<class ModelVariant>
class Taxes : public Scenario<ModelVariant> {
  protected:
    using Scenario<ModelVariant>::model;
    using Scenario<ModelVariant>::id;
    using Scenario<ModelVariant>::scenario_node;
    using Scenario<ModelVariant>::settings;

  public:
    Taxes(const settings::SettingsNode& settings_p, settings::SettingsNode scenario_node_p, Model<ModelVariant>* const model_p);
    Time start() override;
    bool iterate() override;
};
}  // namespace acclimate

#endif
