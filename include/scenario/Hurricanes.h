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

#ifndef ACCLIMATE_HURRICANES_H
#define ACCLIMATE_HURRICANES_H

#include "scenario/RasteredScenario.h"

namespace acclimate {

template<class ModelVariant>
class Hurricanes : public RasteredScenario<ModelVariant> {
  protected:
    FloatType threshold;
    void set_forcing(Region<ModelVariant>* region, const FloatType& forcing_p) const override;
    FloatType get_affected_population_per_cell(const FloatType& x,
                                               const FloatType& y,
                                               const FloatType& population_p,
                                               const FloatType& external_forcing) const override;

  public:
    Hurricanes(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p);
};
}  // namespace acclimate

#endif
