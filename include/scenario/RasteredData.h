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

#ifndef ACCLIMATE_RASTEREDDATA_H
#define ACCLIMATE_RASTEREDDATA_H

#include <cstddef>
#include <iterator>
#include <memory>
#include <string>

#include "netcdftools.h"
#include "types.h"

namespace acclimate {

template<typename T>
class RasteredData {
  public:
    class iterator {
      private:
        FloatType l;
        std::size_t c;
        const FloatType gridsize;
        const std::size_t count;

      public:
        using iterator_category = std::forward_iterator_tag;
        iterator(FloatType l_, const std::size_t& c_, FloatType gridsize_, const std::size_t& count_) : l(l_), c(c_), gridsize(gridsize_), count(count_) {}
        iterator operator++() {
            if (c < count) {
                c++;
                l += gridsize;
            }
            return *this;
        }
        FloatType operator*() const { return l; }
        bool operator==(const iterator& rhs) const { return c == rhs.c; }
        bool operator!=(const iterator& rhs) const { return c != rhs.c; }
    };

    class X {
      protected:
        const RasteredData& rd;

      public:
        explicit X(const RasteredData& rd_) : rd(rd_) {}
        iterator begin() const { return iterator(rd.t_x_min, 0, rd.t_x_gridsize, rd.x_count); }
        iterator end() const { return iterator(rd.t_x_max, rd.x_count, rd.t_x_gridsize, rd.x_count); }
    };

    class Y {
      protected:
        const RasteredData& rd;

      public:
        explicit Y(const RasteredData& rd_) : rd(rd_) {}
        iterator begin() const { return iterator(rd.t_y_min, 0, rd.t_y_gridsize, rd.y_count); }
        iterator end() const { return iterator(rd.t_y_max, rd.y_count, rd.t_y_gridsize, rd.y_count); }
    };

  protected:
    std::unique_ptr<T[]> data;
    FloatType x_min = 0;
    FloatType x_max = 0;
    FloatType x_gridsize = 0;
    FloatType t_x_gridsize = 0;
    FloatType t_x_min = 0;
    FloatType t_x_max = 0;
    std::size_t x_count = 0;
    FloatType y_min = 0;
    FloatType y_max = 0;
    FloatType y_gridsize = 0;
    FloatType t_y_gridsize = 0;
    FloatType t_y_min = 0;
    FloatType t_y_max = 0;
    std::size_t y_count = 0;
    const std::string filename;

  public:
    const X x;
    const Y y;

  protected:
    void read_boundaries(const netCDF::NcFile* file);
    unsigned int x_index(FloatType x_var) const;
    unsigned int y_index(FloatType y_var) const;
    RasteredData();
    explicit RasteredData(std::string filename_p);

  public:
    RasteredData(std::string filename_p, const std::string& variable_name);
    virtual ~RasteredData() = default;
    FloatType abs_x_gridsize() const { return t_x_gridsize; }
    FloatType abs_y_gridsize() const { return t_y_gridsize; }
    template<typename T2>
    FloatType operator/(const RasteredData<T2>& other) const;
    template<typename T2>
    bool is_compatible(const RasteredData<T2>& other) const;
    T read(FloatType x_var, FloatType y_var) const;
    virtual std::string id() const { return "RASTER " + filename; }
};
}  // namespace acclimate

#endif
