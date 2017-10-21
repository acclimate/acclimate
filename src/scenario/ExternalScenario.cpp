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

#include "scenario/ExternalScenario.h"

#include <sstream>

#include "model/EconomicAgent.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "netcdf_headers.h"
#include "scenario/ExternalForcing.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
ExternalScenario<ModelVariant>::ExternalScenario(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p)
    : Scenario<ModelVariant>(settings_p, model_p) {}

template<class ModelVariant>
std::string ExternalScenario<ModelVariant>::fill_template(const std::string& in) const {
    const char* beg_mark = "[[";
    const char* end_mark = "]]";
    std::ostringstream ss;
    std::size_t pos = 0;
    while (true) {
        std::size_t start = in.find(beg_mark, pos);
        std::size_t stop = in.find(end_mark, start);
        if (stop == std::string::npos) {
            break;
        }
        ss.write(&*in.begin() + pos, start - pos);
        start += strlen(beg_mark);
        std::string key = in.substr(start, stop - start);
        if (key != "index") {
            ss << settings["scenario"]["parameters"][key.c_str()].template as<std::string>();
        } else {
            ss << file_index;
        }
        pos = stop + strlen(end_mark);
    }
    ss << in.substr(pos, std::string::npos);
    return ss.str();
}

static unsigned int get_ref_year(const std::string& time_str) {
    if (time_str.substr(0, 11) == "days since ") {
        if (time_str.substr(15, 4) != "-1-1" && time_str.substr(15, 6) != "-01-01") {
            error_("Forcing file has invalid time units");
        }
        return std::stoi(time_str.substr(11, 4));
    }
    if (time_str.substr(0, 14) == "seconds since ") {
        if (time_str.substr(19, 13) != "-1-1 00:00:00" && time_str.substr(19, 15) != "-01-01 00:00:00") {
            error_("Forcing file has invalid time units");
        }
        return std::stoi(time_str.substr(14, 4));
    }
    error_("Forcing file has invalid time units");
}

template<class ModelVariant>
bool ExternalScenario<ModelVariant>::next_forcing_file() {
    if (file_index > file_index_to) {
        forcing.reset();
        return false;
    }
    if (remove_afterwards) {
        remove(forcing_file.c_str());
    }
    if (!expression.empty()) {
        const std::string final_expression = fill_template(expression);
        info("Invoking '" << final_expression << "'");
        if (system(final_expression.c_str()) != 0) {
            error("Invoking '" << final_expression << "' raised an error");
        }
    }
    const std::string filename = fill_template(forcing_file);
    forcing.reset(read_forcing_file(filename, variable_name));
    const std::string new_calendar_str = forcing->calendar_str();
    if (!calendar_str_.empty() && new_calendar_str != calendar_str_) {
        error("Forcing files differ in calendar");
    }
    calendar_str_ = new_calendar_str;
    const std::string new_time_units_str = forcing->time_units_str();
    if (new_time_units_str.substr(0, 14) == "seconds since ") {
        time_step_width = 24 * 60 * 60;
    } else {
        time_step_width = 1;
    }
    if (!time_units_str_.empty() && new_time_units_str != time_units_str_) {
        const unsigned int ref_year = get_ref_year(time_units_str_);
        const unsigned int new_ref_year = get_ref_year(new_time_units_str);
        if (new_ref_year != ref_year + 1) {
            error("Forcing files differ by more than a year");
        }
        time_offset = model->time() + model->delta_t();
    }
    time_units_str_ = new_time_units_str;
    next_time = Time(forcing->next_timestep() / time_step_width);
    if (next_time < 0) {
        error("Empty forcing in " << filename);
    }
    next_time += time_offset;
    file_index++;
    return true;
}

template<class ModelVariant>
Time ExternalScenario<ModelVariant>::start() {
    const settings::SettingsNode& scenario_node = settings["scenario"];

    if (scenario_node.has("start")) {
        start_time = scenario_node["start"].as<Time>();
    }
    if (scenario_node.has("stop")) {
        stop_time = scenario_node["stop"].as<Time>();
        stop_time_known = true;
    } else {
        stop_time_known = false;
    }

    internal_start();

    const settings::SettingsNode& forcing_node = scenario_node["forcing"];
    variable_name = forcing_node["variable"].as<std::string>();
    forcing_file = forcing_node["file"].as<std::string>();

    remove_afterwards = forcing_node["remove"].as<bool>(false);
    file_index_from = forcing_node["index_from"].as<int>(0);
    file_index_to = forcing_node["index_to"].as<int>(file_index_from);
    file_index = file_index_from;

    if (forcing_node.has("expression")) {
        expression = forcing_node["expression"].as<std::string>();
        if (system(nullptr) == 0) {
            error("Cannot invoke system commands");
        }
    } else {
        expression = "";
    }

    if (!next_forcing_file()) {
        error("Empty forcing");
    }

    return start_time;
}

template<class ModelVariant>
void ExternalScenario<ModelVariant>::end() {
    forcing.reset();
    if (remove_afterwards) {
        remove(forcing_file.c_str());
    }
}

template<class ModelVariant>
bool ExternalScenario<ModelVariant>::iterate() {
    if (stop_time_known && model->time() > stop_time) {
        return false;
    }

    if (is_first_timestep()) {
        iterate_first_timestep();
    }
    if (model->time() == next_time) {
        read_forcings();
        next_time = Time(forcing->next_timestep() / time_step_width);
        if (next_time < 0) {
            if (!next_forcing_file() && !stop_time_known) {
                stop_time = model->time();
                stop_time_known = true;
            }
        } else {
            next_time += time_offset;
        }
    }
    return internal_iterate();
}

INSTANTIATE_BASIC(ExternalScenario);
}  // namespace acclimate
