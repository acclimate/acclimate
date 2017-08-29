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

#ifndef ACCLIMATE_RASTEREDTIMEDATA_H
#define ACCLIMATE_RASTEREDTIMEDATA_H

#include "scenario/RasteredData.h"

namespace acclimate {

template<typename T>
class RasteredTimeData : public RasteredData<T> {
  protected:
    using RasteredData<T>::read_boundaries;
    using RasteredData<T>::data;
    using RasteredData<T>::x_count;
    using RasteredData<T>::y_count;
    using RasteredData<T>::filename;
    TimeStep time_index;
    TimeStep time_index_count;
    std::unique_ptr<NcFile> file;
    NcVar variable;
    NcVar time_variable;

  public:
    RasteredTimeData(const std::string& filename_p, const std::string& variable_name);
    virtual ~RasteredTimeData(){};
    int next();
    const std::string calendar_str() const;
    const std::string time_units_str() const;
};
}  // namespace acclimate

#endif
