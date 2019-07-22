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

#include "output/ProgressOutput.h"
#include <iostream>
#include <string>
#include <utility>
#include "model/Model.h"
#include "model/Sector.h"
#include "progressbar.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
ProgressOutput<ModelVariant>::ProgressOutput(const settings::SettingsNode& settings_p,
                                             Model<ModelVariant>* model_p,
                                             Scenario<ModelVariant>* scenario_p,
                                             settings::SettingsNode output_node_p)
    : Output<ModelVariant>(settings_p, model_p, scenario_p, std::move(output_node_p)) {}

template<class ModelVariant>
void ProgressOutput<ModelVariant>::initialize() {
    const auto total = output_node["total"].template as<std::size_t>();
    bar.reset(new progressbar::ProgressBar(total));
}

template<class ModelVariant>
void ProgressOutput<ModelVariant>::checkpoint_stop() {
    bar->println("     [ Checkpointing ]", false);
    bar->abort();
}

template<class ModelVariant>
void ProgressOutput<ModelVariant>::checkpoint_resume() {
    bar->reset_eta();
    bar->resume();
}

template<class ModelVariant>
void ProgressOutput<ModelVariant>::internal_end() {
    bar->close();
}

template<class ModelVariant>
void ProgressOutput<ModelVariant>::internal_iterate_end() {
    ++(*bar);
}

INSTANTIATE_BASIC(ProgressOutput);
}  // namespace acclimate
