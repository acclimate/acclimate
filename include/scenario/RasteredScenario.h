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

#include "scenario/RasteredData.h"
#include "scenario/RasteredTimeData.h"
#include "scenario/Scenario.h"

namespace acclimate {

template<class ModelVariant>
class Region;

template<class ModelVariant>
class RasteredScenario : public Scenario<ModelVariant> {
  public:
    struct RegionInfo {
        Region<ModelVariant>* region;
        FloatType population;
        FloatType people_affected;
        FloatType forcing;
    };

  protected:
    using Scenario<ModelVariant>::settings;
    using Scenario<ModelVariant>::set_firm_property;
    using Scenario<ModelVariant>::set_consumer_property;
    using Scenario<ModelVariant>::start_time;
    using Scenario<ModelVariant>::stop_time;

    std::string forcing_file;
    std::string expression;
    std::string variable_name;
    bool remove_afterwards = false;
    unsigned int file_index_from = 0;
    unsigned int file_index_to = 0;
    unsigned int file_index = 0;
    std::string calendar_str_;
    std::string time_units_str_;
    Time next_time = Time(0.0);
    Time time_offset = Time(0.0);
    int time_step_width = 1;
    bool stop_time_known = false;

    std::unique_ptr<RasteredTimeData<FloatType>> forcing;
    std::unique_ptr<RasteredData<int>> iso_raster;
    std::unique_ptr<RasteredData<FloatType>> population;
    FloatType people_affected_ = 0;
    std::vector<RegionInfo> region_forcings;
    bool next_forcing_file();
    virtual void set_forcing(Region<ModelVariant>* region, const FloatType& forcing_) const = 0;
    virtual FloatType get_affected_population_per_cell(const FloatType& x,
                                                       const FloatType& y,
                                                       const FloatType& population,
                                                       const FloatType& external_forcing) const = 0;
    std::string fill_template(const std::string& in) const;
    RasteredScenario(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p);

  public:
    using Scenario<ModelVariant>::model;
    using Scenario<ModelVariant>::is_first_timestep;
    virtual ~RasteredScenario(){};
    bool iterate() override;
    Time start() override;
    void end() override;
    inline const std::vector<RegionInfo>& forcings() const { return region_forcings; }
    std::string calendar_str() const override { return calendar_str_; };
    std::string time_units_str() const override { return time_units_str_; };
    inline FloatType people_affected() const { return people_affected_; };
};
}  // namespace acclimate

#endif
