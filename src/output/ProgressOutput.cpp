// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "output/ProgressOutput.h"

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
