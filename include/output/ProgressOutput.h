// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_PROGRESSOUTPUT_H
#define ACCLIMATE_PROGRESSOUTPUT_H

#include "output/Output.h"
#include "progressbar.h"

namespace acclimate {

class Model;

class ProgressOutput final : public Output {
  private:
    progressbar::ProgressBar bar;

  public:
    ProgressOutput(Model* model);
    void iterate() override;
    void end() override;
    void checkpoint_resume() override;
    void checkpoint_stop() override;
};
}  // namespace acclimate

#endif
