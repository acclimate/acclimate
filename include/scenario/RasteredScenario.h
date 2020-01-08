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

#ifndef ACCLIMATE_RASTEREDSCENARIO_H
#define ACCLIMATE_RASTEREDSCENARIO_H

#include <memory>
#include <string>
#include <vector>

#include "scenario/ExternalScenario.h"
#include "scenario/RasteredData.h"

namespace acclimate {

class Region;

template<class RegionForcingType>
class RasteredScenario : public ExternalScenario {
  public:
    struct RegionInfo {
        Region* region;
        FloatType proxy_sum;
        RegionForcingType forcing;
    };

  protected:
    using ExternalScenario::forcing;
    using ExternalScenario::next_time;
    using ExternalScenario::scenario_node;
    using ExternalScenario::settings;
    std::unique_ptr<RasteredData<int>> iso_raster;
    std::unique_ptr<RasteredData<FloatType>> proxy;
    std::vector<RegionInfo> region_forcings;
    FloatType total_current_proxy_sum_ = 0;

  protected:
    RasteredScenario(const settings::SettingsNode& settings_p, settings::SettingsNode scenario_node_p, Model* model_p);
    virtual RegionForcingType new_region_forcing(Region* region) const = 0;
    virtual void set_region_forcing(Region* region, const RegionForcingType& forcing, FloatType proxy_sum) const = 0;
    virtual void reset_forcing(Region* region, RegionForcingType& forcing) const = 0;
    virtual void add_cell_forcing(
        FloatType x, FloatType y, FloatType proxy_value, FloatType cell_forcing, const Region* region, RegionForcingType& region_forcing) const = 0;
    void internal_start() override;
    void internal_iterate_start() override;
    bool internal_iterate_end() override;
    void iterate_first_timestep() override;
    ExternalForcing* read_forcing_file(const std::string& filename, const std::string& variable_name) override;
    void read_forcings() override;

  public:
    ~RasteredScenario() override = default;
    using ExternalScenario::id;
    using ExternalScenario::model;
    const std::vector<RegionInfo>& forcings() const { return region_forcings; }
};
}  // namespace acclimate

#endif
