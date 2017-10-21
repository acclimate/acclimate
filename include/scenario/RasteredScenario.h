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

template<class ModelVariant>
class RasteredScenario : public ExternalScenario<ModelVariant> {
  public:
    struct RegionInfo {
        Region<ModelVariant>* region;
        FloatType population;
        FloatType people_affected;
        FloatType forcing;
    };

  protected:
    using ExternalScenario<ModelVariant>::forcing;
    using ExternalScenario<ModelVariant>::model;
    using ExternalScenario<ModelVariant>::settings;

    std::unique_ptr<RasteredData<int>> iso_raster;
    std::unique_ptr<RasteredData<FloatType>> population;
    FloatType people_affected_ = 0;
    std::vector<RegionInfo> region_forcings;

    virtual void set_forcing(Region<ModelVariant>* region, const FloatType& forcing_) const = 0;
    virtual FloatType get_affected_population_per_cell(const FloatType& x,
                                                       const FloatType& y,
                                                       const FloatType& population,
                                                       const FloatType& external_forcing) const = 0;
    void internal_start() override;
    bool internal_iterate() override;
    void iterate_first_timestep() override;
    ExternalForcing* read_forcing_file(const std::string& filename, const std::string& variable_name) override;
    void read_forcings() override;
    RasteredScenario(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p);

  public:
    virtual ~RasteredScenario(){};
    inline const std::vector<RegionInfo>& forcings() const { return region_forcings; }
    inline FloatType people_affected() const { return people_affected_; };
};
}  // namespace acclimate

#endif
