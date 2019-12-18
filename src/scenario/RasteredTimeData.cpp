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

#include "scenario/RasteredTimeData.h"

#include <memory>

#include "types.h"

namespace acclimate {

template<typename T>
RasteredTimeData<T>::RasteredTimeData(const std::string& filename_p, const std::string& variable_name)
    : RasteredData<T>::RasteredData(filename_p), ExternalForcing::ExternalForcing(filename_p, variable_name) {
    read_boundaries(file.get());
    data.reset(new T[y_count * x_count]);
}

template<typename T>
void RasteredTimeData<T>::read_data() {
    variable.getVar({time_index, 0, 0}, {1, y_count, x_count}, data.get());
}

template class RasteredTimeData<int>;

template class RasteredTimeData<FloatType>;
}  // namespace acclimate
