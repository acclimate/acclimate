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

#ifndef ACCLIMATE_EXTERNALFORCING_H
#define ACCLIMATE_EXTERNALFORCING_H

#include "netcdf_headers.h"
#include "scenario/ExternalForcing.h"
#include "scenario/RasteredData.h"

namespace acclimate {

class ExternalForcing {
  protected:
    std::string filename;
    TimeStep time_index;
    TimeStep time_index_count;
    std::unique_ptr<netCDF::NcFile> file;
    netCDF::NcVar variable;
    netCDF::NcVar time_variable;
    virtual void read_data() = 0;

  public:
    ExternalForcing(std::string filename_p, const std::string& variable_name);
    virtual ~ExternalForcing(){};
    int next_timestep();
    const std::string calendar_str() const;
    const std::string time_units_str() const;
#ifdef DEBUG
    virtual inline explicit operator std::string() const { return "FORCING"; }
#endif
};
}  // namespace acclimate

#endif
