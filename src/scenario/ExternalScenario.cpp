// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "scenario/ExternalScenario.h"

#include "acclimate.h"
#include "model/EconomicAgent.h"
#include "model/Model.h"
#include "model/Region.h"
#include "scenario/ExternalForcing.h"
#include "settingsnode.h"

namespace acclimate {

ExternalScenario::ExternalScenario(const settings::SettingsNode& settings, settings::SettingsNode scenario_node_, Model* model)
    : Scenario(settings, std::move(scenario_node_), model) {}

auto ExternalScenario::fill_template(const std::string& in) const -> std::string {
    const char* beg_mark = "[[";
    const char* end_mark = "]]";
    std::ostringstream ss;
    std::size_t pos = 0;
    while (true) {
        std::size_t start = in.find(beg_mark, pos);
        std::size_t const stop = in.find(end_mark, start);
        if (stop == std::string::npos) {
            break;
        }
        ss.write(&*in.begin() + pos, start - pos);
        start += std::strlen(beg_mark);
        std::string const key = in.substr(start, stop - start);
        if (key != "index") {
            ss << scenario_node_["parameters"][key.c_str()].as<std::string>();
        } else {
            ss << file_index_;
        }
        pos = stop + std::strlen(end_mark);
    }
    ss << in.substr(pos, std::string::npos);
    return ss.str();
}

auto ExternalScenario::get_ref_year(const std::string& filename, const std::string& time_str) -> unsigned int {
    if (time_str.substr(0, 11) == "days since ") {
        if (time_str.substr(15, 4) != "-1-1" && time_str.substr(15, 6) != "-01-01") {
            throw log::error(this, "Forcing file ", filename, " has invalid time units");
        }
        return std::stoi(time_str.substr(11, 4));
    }
    if (time_str.substr(0, 14) == "seconds since ") {
        if (time_str.substr(19, 13) != "-1-1 00:00:00" && time_str.substr(19, 15) != "-01-01 00:00:00") {
            throw log::error(this, "Forcing file ", filename, " has invalid time units");
        }
        return std::stoi(time_str.substr(14, 4));
    }
    throw log::error(this, "Forcing file ", filename, " has invalid time units");
}

auto ExternalScenario::next_forcing_file() -> bool {
    if (file_index_ > file_index_to_) {
        forcing_.reset();
        return false;
    }
    if (remove_afterwards_) {
        std::remove(forcing_file_.c_str());
    }
    if (!expression_.empty()) {
        const std::string final_expression = fill_template(expression_);
        log::info(this, "Invoking '", final_expression, "'");
        if (std::system(final_expression.c_str()) != 0) {  // NOLINT(cert-env33-c)
            throw log::error(this, "Invoking '", final_expression, "' raised an error");
        }
    }
    const std::string filename = fill_template(forcing_file_);
    forcing_.reset(read_forcing_file(filename, variable_name_));
    const std::string new_calendar_str = forcing_->calendar_str();
    if (!calendar_str_.empty() && new_calendar_str != calendar_str_) {
        throw log::error(this, "Forcing files differ in calendar");
    }
    calendar_str_ = new_calendar_str;
    const std::string new_time_units_str = forcing_->time_units_str();
    if (new_time_units_str.substr(0, 14) == "seconds since ") {
        time_step_width_ = 24 * 60 * 60;
    } else {
        time_step_width_ = 1;
    }
    if (!time_units_str_.empty() && new_time_units_str != time_units_str_) {
        const unsigned int ref_year = get_ref_year(filename, time_units_str_);
        const unsigned int new_ref_year = get_ref_year(filename, new_time_units_str);
        if (new_ref_year != ref_year + 1) {
            throw log::error(this, "Forcing files differ by more than a year");
        }
        time_offset_ = model()->time() + model()->delta_t();
    }
    time_units_str_ = new_time_units_str;
    next_time_ = Time(forcing_->next_timestep()) / time_step_width_;
    if (next_time_ < 0) {
        throw log::error(this, "Empty forcing_ in ", filename);
    }
    next_time_ += time_offset_;
    file_index_++;
    return true;
}

void ExternalScenario::start() {
    internal_start();

    const settings::SettingsNode& forcing_node = scenario_node_["forcing_"];
    variable_name_ = forcing_node["variable"].as<std::string>();
    forcing_file_ = forcing_node["file"].as<std::string>();

    remove_afterwards_ = forcing_node["remove"].as<bool>(false);
    file_index_from_ = forcing_node["index_from"].as<int>(0);
    file_index_to_ = forcing_node["index_to"].as<int>(file_index_from_);
    file_index_ = file_index_from_;

    if (forcing_node.has("expression_")) {
        expression_ = forcing_node["expression_"].as<std::string>();
        if (system(nullptr) == 0) {
            throw log::error(this, "Cannot invoke system commands");
        }
    } else {
        expression_ = "";
    }

    if (!next_forcing_file()) {
        throw log::error(this, "Empty forcing_");
    }
}

void ExternalScenario::end() {
    forcing_.reset();
    if (remove_afterwards_) {
        remove(forcing_file_.c_str());
    }
}

void ExternalScenario::iterate() {
    if (done_) {
        return;
    }

    if (model()->is_first_timestep()) {
        iterate_first_timestep();
    }

    internal_iterate_start();
    if (next_time_ < 0) {
        if (!next_forcing_file()) {
            done_ = true;
            for (const auto& region : model()->regions) {
                for (const auto& ea : region->economic_agents) {
                    ea->set_forcing(static_cast<Forcing>(1.0));
                }
            }
            return;
        }
    }
    if (model()->time() >= next_time_) {
        read_forcings();
        next_time_ = Time(forcing_->next_timestep()) / time_step_width_;
        if (next_time_ >= 0) {
            next_time_ += time_offset_;
        }
    }
    internal_iterate_end();
}

}  // namespace acclimate
