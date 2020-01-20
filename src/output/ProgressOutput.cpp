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

namespace acclimate {

ProgressOutput::ProgressOutput(const settings::SettingsNode& settings_p, Model* model_p, settings::SettingsNode output_node_p)
    : Output(settings_p, model_p, std::move(output_node_p)) {}

void ProgressOutput::initialize() {
    const auto total = output_node["total"].template as<std::size_t>();
    bar = std::make_unique<progressbar::ProgressBar>(total);
}

void ProgressOutput::checkpoint_stop() {
    bar->println("     [ Checkpointing ]", false);
    bar->abort();
}

void ProgressOutput::checkpoint_resume() {
    bar->reset_eta();
    bar->resume();
}

void ProgressOutput::internal_end() { bar->close(); }

void ProgressOutput::internal_iterate_end() { ++(*bar); }

}  // namespace acclimate
