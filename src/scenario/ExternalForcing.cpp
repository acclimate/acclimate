// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "scenario/ExternalForcing.h"

#include "netcdfpp.h"

namespace acclimate {

ExternalForcing::ExternalForcing(std::string filename, std::string variable_name) : time_index_(0) {
    file_.open(std::move(filename), 'r');
    variable_ = std::make_unique<netCDF::Variable>(file_.variable(std::move(variable_name)).require());
    time_variable_ = std::make_unique<netCDF::Variable>(file_.variable("time").require());
    time_index_count_ = time_variable_->size();
}

ExternalForcing::~ExternalForcing() = default;  // needed to use forward declares for std::unique_ptr

auto ExternalForcing::next_timestep() -> int {
    if (time_index_ >= time_index_count_) {
        return -1;
    }
    read_data();
    auto day = time_variable_->get<int, 1>({time_index_});
    time_index_++;
    return day;
}

auto ExternalForcing::calendar_str() const -> std::string { return time_variable_->attribute("calendar").require().get_string(); }

auto ExternalForcing::time_units_str() const -> std::string { return time_variable_->attribute("units").require().get_string(); }
}  // namespace acclimate
