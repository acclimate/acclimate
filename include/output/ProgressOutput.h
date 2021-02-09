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

#include "output/Output.h"
#include "progressbar.h"

namespace acclimate {

class Model;

class ProgressOutput final : public Output {
  private:
    progressbar::ProgressBar bar;

  public:
    ProgressOutput(Model* model_p);
    void iterate() override;
    void end() override;
    void checkpoint_resume() override;
    void checkpoint_stop() override;
};
}  // namespace acclimate

#endif
