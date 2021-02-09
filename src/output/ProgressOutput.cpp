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

#include <memory>

#include "ModelRun.h"
#include "model/Model.h"
#include "progressbar.h"

namespace acclimate {

ProgressOutput::ProgressOutput(Model* model_p) : Output(model_p), bar(model_p->run()->total_timestep_count()) {}

void ProgressOutput::checkpoint_stop() {
    bar.println("     [ Checkpointing ]", false);
    bar.abort();
}

void ProgressOutput::checkpoint_resume() {
    bar.reset_eta();
    bar.resume();
}

void ProgressOutput::end() { bar.close(); }

void ProgressOutput::iterate() { ++bar; }

}  // namespace acclimate
