/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
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

#include <memory>
#include <string>

#include "acclimate.h"
#include "scenario/Scenario.h"

namespace settings {
class SettingsNode;
}  // namespace settings

namespace acclimate {

class ExternalForcing;  // IWYU pragma: keep
class Model;

class ExternalScenario : public Scenario {
  protected:
    std::string forcing_file;
    std::string expression;
    std::string variable_name;
    bool remove_afterwards = false;
    bool done = false;
    unsigned int file_index_from = 0;
    unsigned int file_index_to = 0;
    unsigned int file_index = 0;
    std::string calendar_str_;
    std::string time_units_str_;
    Time next_time = Time(0.0);
    Time time_offset = Time(0.0);
    int time_step_width = 1;
    std::unique_ptr<ExternalForcing> forcing;

  protected:
    ExternalScenario(const settings::SettingsNode& settings_p, settings::SettingsNode scenario_node_p, Model* model_p);
    bool next_forcing_file();
    std::string fill_template(const std::string& in) const;
    unsigned int get_ref_year(const std::string& filename, const std::string& time_str);
    virtual void internal_start() {}
    virtual void internal_iterate_start() {}
    virtual void internal_iterate_end() {}
    virtual void iterate_first_timestep() {}
    virtual ExternalForcing* read_forcing_file(const std::string& filename, const std::string& variable_name) = 0;
    virtual void read_forcings() = 0;

  public:
    virtual ~ExternalScenario() override = default;
    void iterate() override;
    void start() override;
    void end() override;
    std::string calendar_str() const override { return calendar_str_; }
    std::string time_units_str() const override { return time_units_str_; }
};
}  // namespace acclimate

#endif
