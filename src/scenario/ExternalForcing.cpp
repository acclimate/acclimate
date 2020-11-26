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

#include "scenario/ExternalForcing.h"

#include <utility>

#include "netcdfpp.h"

namespace acclimate {

ExternalForcing::ExternalForcing(std::string filename, std::string variable_name) {
    file.open(std::move(filename), 'r');
    variable = std::make_unique<netCDF::Variable>(file.variable(std::move(variable_name)).require());
    time_variable = std::make_unique<netCDF::Variable>(file.variable("time").require());
    time_index_count = time_variable->size();
    time_index = 0;
}

ExternalForcing::~ExternalForcing() = default;  // needed to use forward declares for std::unique_ptr

int ExternalForcing::next_timestep() {
    if (time_index >= time_index_count) {
        return -1;
    }
    read_data();
    auto day = time_variable->get<int, 1>({time_index});
    time_index++;
    return day;
}

std::string ExternalForcing::calendar_str() const { return time_variable->attribute("calendar").require().get_string(); }

std::string ExternalForcing::time_units_str() const { return time_variable->attribute("units").require().get_string(); }
}  // namespace acclimate
