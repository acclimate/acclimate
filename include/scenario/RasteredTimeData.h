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

#include <string>

#include "scenario/ExternalForcing.h"
#include "scenario/RasteredData.h"

namespace acclimate {

template<typename T>
class RasteredTimeData : public RasteredData<T>, public ExternalForcing {
  private:
    using RasteredData<T>::read_boundaries;
    using RasteredData<T>::data;
    using RasteredData<T>::id;
    using RasteredData<T>::x_count;
    using RasteredData<T>::y_count;
    using RasteredData<T>::filename;
    using ExternalForcing::time_index;
    using ExternalForcing::variable;

  private:
    void read_data() override;

  public:
    RasteredTimeData(const std::string& filename_p, const std::string& variable_name);
};
}  // namespace acclimate

#endif
