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

#ifndef ACCLIMATE_DAMAGEOUTPUT_H
#define ACCLIMATE_DAMAGEOUTPUT_H

#include <fstream>
#include <memory>

#include "output/Output.h"

namespace acclimate {

class Model;
class Region;
class Sector;
class Scenario;

class DamageOutput : public Output {
  public:
    using Output::output_node;

  private:
    FlowQuantity damage;
    std::ostream* out;
    std::unique_ptr<std::ofstream> outfile;

  private:
    void internal_iterate_end() override;
    void internal_end() override;

  public:
    DamageOutput(const settings::SettingsNode& settings_p, Model* model_p, Scenario* scenario_p, settings::SettingsNode output_node_p);
    void initialize() override;
    using Output::model;
};
}  // namespace acclimate

#endif
