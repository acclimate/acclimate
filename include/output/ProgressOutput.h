/*
  Copyright (C) 2014-2018 Sven Willner <sven.willner@pik-potsdam.de>
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

#ifndef ACCLIMATE_PROGRESSOUTPUT_H
#define ACCLIMATE_PROGRESSOUTPUT_H

#include <memory>
#include "output/Output.h"

namespace progressbar {
class ProgressBar;
}  // namespace progressbar

namespace acclimate {

template<class ModelVariant>
class Model;
template<class ModelVariant>
class Sector;
template<class ModelVariant>
class Scenario;

template<class ModelVariant>
class ProgressOutput : public Output<ModelVariant> {
  public:
    using Output<ModelVariant>::id;
    using Output<ModelVariant>::model;
    using Output<ModelVariant>::output_node;

  protected:
    std::unique_ptr<progressbar::ProgressBar> bar;

  protected:
    void internal_iterate_end() override;
    void internal_end() override;

  public:
    ProgressOutput(const settings::SettingsNode& settings_p,
                   Model<ModelVariant>* model_p,
                   Scenario<ModelVariant>* scenario_p,
                   settings::SettingsNode output_node_p);
    void initialize() override;
    void checkpoint_resume() override;
};
}  // namespace acclimate

#endif
