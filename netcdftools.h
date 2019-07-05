/*
  Copyright (C) 2019 Sven Willner <sven.willner@gmail.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NETCDFTOOLS_H
#define NETCDFTOOLS_H

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wunused-variable"

#if _MSC_VER || __INTEL_COMPILER
#pragma warning push
#pragma warning disable : 858
#endif

#include <ncDim.h>
#include <ncFile.h>
#include <ncGroupAtt.h>
#include <ncType.h>
#include <ncVar.h>
#include <netcdf>

#if _MSC_VER || __INTEL_COMPILER
#pragma warning pop
#endif

#pragma GCC diagnostic pop

#include <algorithm>
#include <string>
#include <vector>

inline bool check_dimensions(const netCDF::NcVar& var, const std::vector<std::string>& names) {
    const auto& dims = var.getDims();
    if (dims.size() != names.size()) {
        return false;
    }
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (!names[i].empty() && dims[i].getName() != names[i]) {
            return false;
        }
    }
    return true;
}

#endif
