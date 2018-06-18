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

#include "scenario/ExternalScenario.h"
#include "scenario/RasteredData.h"

namespace acclimate {

template<class ModelVariant>
class Region;

template<class ModelVariant, class RegionForcingType>
class RasteredScenario : public ExternalScenario<ModelVariant> {
  public:
    struct RegionInfo {
        Region<ModelVariant>* region;
        FloatType proxy_sum;
        RegionForcingType forcing;
    };

  protected:
    using ExternalScenario<ModelVariant>::forcing;
    using ExternalScenario<ModelVariant>::model;
    using ExternalScenario<ModelVariant>::next_time;
    using ExternalScenario<ModelVariant>::settings;

    std::unique_ptr<RasteredData<int>> iso_raster;
    std::unique_ptr<RasteredData<FloatType>> proxy;
    std::vector<RegionInfo> region_forcings;
    FloatType total_current_proxy_sum_;

    virtual RegionForcingType new_region_forcing(Region<ModelVariant>* region) const = 0;
    virtual void set_region_forcing(Region<ModelVariant>* region, const RegionForcingType& forcing, FloatType proxy_sum) const = 0;
    virtual void reset_forcing(Region<ModelVariant>* region, RegionForcingType& forcing) const = 0;
    virtual void add_cell_forcing(FloatType x,
                                  FloatType y,
                                  FloatType proxy_value,
                                  FloatType cell_forcing,
                                  const Region<ModelVariant>* region,
                                  RegionForcingType& region_forcing) const = 0;
    void internal_start() override;
    void internal_iterate_start() override;
    bool internal_iterate_end() override;
    void iterate_first_timestep() override;
    ExternalForcing* read_forcing_file(const std::string& filename, const std::string& variable_name) override;
    void read_forcings() override;
    RasteredScenario(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p);

  public:
    using ExternalScenario<ModelVariant>::id;
    virtual ~RasteredScenario(){};
    inline const std::vector<RegionInfo>& forcings() const { return region_forcings; }
    inline FloatType total_current_proxy_sum() const { return total_current_proxy_sum_; };
};
}  // namespace acclimate

#endif
