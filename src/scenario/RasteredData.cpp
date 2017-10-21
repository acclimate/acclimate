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

#include "scenario/RasteredData.h"

#include <utility>

namespace acclimate {

template<typename T>
void RasteredData<T>::read_boundaries(const netCDF::NcFile* file) {
    {
        netCDF::NcVar x_var = file->getVar("x");
        if (x_var.isNull()) {
            x_var = file->getVar("lon");
            if (x_var.isNull()) {
                x_var = file->getVar("longitude");
                if (x_var.isNull()) {
                    error("No longitude variable found in '" << filename << "'");
                }
            }
        }
        x_var.getVar({0}, &x_min);
        x_count = x_var.getDim(0).getSize();
        x_var.getVar({x_count - 1}, &x_max);
        FloatType tmp;
        x_var.getVar({1}, &tmp);
        x_gridsize = tmp - x_min;
        t_x_min = std::min(x_min, x_max);
        t_x_max = std::max(x_min, x_max);
        t_x_gridsize = std::abs(x_gridsize);
    }
    {
        netCDF::NcVar y_var = file->getVar("y");
        if (y_var.isNull()) {
            y_var = file->getVar("lat");
            if (y_var.isNull()) {
                y_var = file->getVar("latitude");
                if (y_var.isNull()) {
                    error("No latitude variable found in '" << filename << "'");
                }
            }
        }
        y_var.getVar({0}, &y_min);
        y_count = y_var.getDim(0).getSize();
        y_var.getVar({y_count - 1}, &y_max);
        FloatType tmp;
        y_var.getVar({1}, &tmp);
        y_gridsize = tmp - y_min;
        t_y_min = std::min(y_min, y_max);
        t_y_max = std::max(y_min, y_max);
        t_y_gridsize = std::abs(y_gridsize);
    }
}

template<typename T>
RasteredData<T>::RasteredData() : x(*this), y(*this) {}

template<typename T>
RasteredData<T>::RasteredData(std::string filename_p) : filename(std::move(filename_p)), x(*this), y(*this) {}

template<typename T>
RasteredData<T>::RasteredData(std::string filename_p, const std::string& variable_name) : filename(std::move(filename_p)), x(*this), y(*this) {
    std::unique_ptr<netCDF::NcFile> file;
    try {
        file.reset(new netCDF::NcFile(filename, netCDF::NcFile::read));
    } catch (netCDF::exceptions::NcException& ex) {
        error("Could not open '" + filename + "'");
    }
    netCDF::NcVar variable = file->getVar(variable_name);
    if (variable.isNull()) {
        error("Cannot find variable '" << variable_name << "' in '" << filename << "'");
    }
    read_boundaries(file.get());
    data = new T[y_count * x_count];
    variable.getVar({0, 0}, {y_count, x_count}, data);
}

template<typename T>
RasteredData<T>::~RasteredData() {
    delete[] data;
}

template<typename T>
template<typename T2>
FloatType RasteredData<T>::operator/(const RasteredData<T2>& other) const {
    return t_x_gridsize * t_y_gridsize / other.abs_x_gridsize() / other.abs_y_gridsize();
}

template<typename T>
template<typename T2>
bool RasteredData<T>::is_compatible(const RasteredData<T2>& other) const {
    return std::abs(t_x_gridsize - other.abs_x_gridsize()) < 1e-5 && std::abs(t_y_gridsize - other.abs_y_gridsize()) < 1e-5;
}

template<typename T>
inline unsigned int RasteredData<T>::x_index(const FloatType& x_var) const {
    if (x_min < x_max) {
        if (x_var < x_min || x_var > x_max + x_gridsize) {
            return std::numeric_limits<unsigned int>::quiet_NaN();
        }
    } else {
        if (x_var > x_min || x_var < x_max + x_gridsize) {
            return std::numeric_limits<unsigned int>::quiet_NaN();
        }
    }
    return static_cast<unsigned int>((x_var - x_min) * x_count / (x_max - x_min + x_gridsize));
}

template<typename T>
inline unsigned int RasteredData<T>::y_index(const FloatType& y_var) const {
    if (y_min < y_max) {
        if (y_var < y_min || y_var > y_max + y_gridsize) {
            return std::numeric_limits<unsigned int>::quiet_NaN();
        }
    } else {
        if (y_var > y_min || y_var < y_max + y_gridsize) {
            return std::numeric_limits<unsigned int>::quiet_NaN();
        }
    }
    return static_cast<unsigned int>((y_var - y_min) * y_count / (y_max - y_min + y_gridsize));
}

template<typename T>
inline T RasteredData<T>::read(const FloatType& x_var, const FloatType& y_var) const {
    const unsigned int x_i = x_index(x_var);
    if (std::isnan(x_i)) {
        return std::numeric_limits<T>::quiet_NaN();
    }
    const unsigned int y_i = y_index(y_var);
    if (std::isnan(y_i)) {
        return std::numeric_limits<T>::quiet_NaN();
    }
    T res = data[y_i * x_count + x_i];
    if (res > 1e18) {
        res = std::numeric_limits<T>::quiet_NaN();
    }
    return res;
}

template class RasteredData<int>;
template class RasteredData<FloatType>;
template bool RasteredData<FloatType>::is_compatible(const RasteredData<FloatType>& other) const;
template bool RasteredData<FloatType>::is_compatible(const RasteredData<int>& other) const;
template bool RasteredData<int>::is_compatible(const RasteredData<FloatType>& other) const;
template bool RasteredData<int>::is_compatible(const RasteredData<int>& other) const;
template FloatType RasteredData<FloatType>::operator/(const RasteredData<FloatType>& other) const;
template FloatType RasteredData<FloatType>::operator/(const RasteredData<int>& other) const;
template FloatType RasteredData<int>::operator/(const RasteredData<FloatType>& other) const;
template FloatType RasteredData<int>::operator/(const RasteredData<int>& other) const;
}  // namespace acclimate
