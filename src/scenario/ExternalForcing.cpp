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

#include <cstddef>
#include <utility>

#include "acclimate.h"
#include "netcdftools.h"

namespace acclimate {

ExternalForcing::ExternalForcing(std::string filename_p, const std::string& variable_name) : filename(std::move(filename_p)) {
    try {
        file = std::make_unique<netCDF::NcFile>(filename, netCDF::NcFile::read, netCDF::NcFile::nc4);
    } catch (netCDF::exceptions::NcException& ex) {
        throw log::error("Could not open '", filename, "'");
    }
    variable = file->getVar(variable_name);
    time_variable = file->getVar("time");
    time_index_count = time_variable.getDim(0).getSize();
    time_index = 0;
}

int ExternalForcing::next_timestep() {
    if (time_index >= time_index_count) {
        return -1;
    }
    read_data();
    int day;
    time_variable.getVar({time_index}, {1}, &day);
    time_index++;
    return day;
}

std::string ExternalForcing::calendar_str() const {
    try {
        std::string res;
        time_variable.getAtt("calendar").getValues(res);
        return res;
    } catch (netCDF::exceptions::NcException& e) {
        throw log::error("Could not read calendar attribute in ", filename, ": ", e.what());
    }
}

std::string ExternalForcing::time_units_str() const {
    try {
        std::string res;
        time_variable.getAtt("units").getValues(res);
        return res;
    } catch (netCDF::exceptions::NcException& e) {
        throw log::error("could not read time units attribute in ", filename, ": ", e.what());
    }
}
}  // namespace acclimate
