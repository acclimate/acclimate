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

#ifndef ACCLIMATE_JSONNETWORKOUTPUT_H
#define ACCLIMATE_JSONNETWORKOUTPUT_H

#include <string>
#include "output/Output.h"
#include "types.h"

namespace acclimate {

template<class ModelVariant>
class Model;
template<class ModelVariant>
class Scenario;

template<class ModelVariant>
class JSONNetworkOutput : public Output<ModelVariant> {
  public:
    using Output<ModelVariant>::output_node;
    using Output<ModelVariant>::model;

  protected:
    TimeStep timestep = 0;
    std::string filename;
    bool advanced = false;
    void internal_iterate_end() override;

  public:
    JSONNetworkOutput(const settings::SettingsNode& settings_p,
                      Model<ModelVariant>* model_p,
                      Scenario<ModelVariant>* scenario_p,
                      settings::SettingsNode output_node_p);
    void initialize() override;
};
}  // namespace acclimate

#endif
