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

#ifndef ACCLIMATE_FLOODING_H
#define ACCLIMATE_FLOODING_H

#include <vector>
#include "scenario/RasteredScenario.h"

namespace acclimate {

template<class ModelVariant>
class Flooding : public RasteredScenario<ModelVariant, FloatType> {
  protected:
    std::vector<std::size_t> sectors;
    FloatType new_region_forcing(Region<ModelVariant>* region) const override;
    void set_region_forcing(Region<ModelVariant>* region, const FloatType& forcing, FloatType proxy_sum) const override;
    void reset_forcing(Region<ModelVariant>* region, FloatType& forcing) const override;
    void add_cell_forcing(
        FloatType x, FloatType y, FloatType proxy_value, FloatType cell_forcing, const Region<ModelVariant>* region, FloatType& region_forcing) const override;

  public:
    Flooding(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p);
};
}  // namespace acclimate

#endif
