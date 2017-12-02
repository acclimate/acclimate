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

#ifndef ACCLIMATE_EXTERNALSCENARIO_H
#define ACCLIMATE_EXTERNALSCENARIO_H

#include "scenario/ExternalForcing.h"
#include "scenario/Scenario.h"

namespace acclimate {

template<class ModelVariant>
class Region;

template<class ModelVariant>
class ExternalScenario : public Scenario<ModelVariant> {
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

    std::unique_ptr<ExternalForcing> forcing;

    bool next_forcing_file();
    std::string fill_template(const std::string& in) const;
    ExternalScenario(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p);

    virtual void internal_start(){};
    virtual bool internal_iterate() { return true; };
    virtual void iterate_first_timestep(){};
    virtual ExternalForcing* read_forcing_file(const std::string& filename, const std::string& variable_name) = 0;
    virtual void read_forcings() = 0;

  public:
    using Scenario<ModelVariant>::model;
    using Scenario<ModelVariant>::is_first_timestep;
    virtual ~ExternalScenario(){};
    bool iterate() override;
    Time start() override;
    void end() override;
    std::string calendar_str() const override { return calendar_str_; };
    std::string time_units_str() const override { return time_units_str_; };
};
}  // namespace acclimate

#endif
