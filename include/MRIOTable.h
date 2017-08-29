/*
  Copyright (C) 2014-2017 Sven Willner <sven.willner@pik-potsdam.de>

  This file is part of libmrio.

  libmrio is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  libmrio is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with libmrio.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef LIBMRIO_MRIOTABLE_H
#define LIBMRIO_MRIOTABLE_H

#include <iosfwd>
#include <limits>
#include <string>
#include <vector>
#include "MRIOIndexSet.h"
#ifdef DEBUG
#include <cassert>
#else
#ifndef assert
#define assert(a) \
    {}
#endif
#endif

namespace mrio {

template<typename T, typename I>
class Table {
  protected:
    std::vector<T> data;
    IndexSet<I> index_set_;

    void read_indices_from_csv(std::istream& indicesstream);
    void read_data_from_csv(std::istream& datastream, const T& threshold);
    void insert_sector_offset_x_y(const SuperSector<I>* i, const I& i_regions_count, const I& subsectors_count);
    void insert_sector_offset_y(
        const SuperSector<I>* i, const I& i_regions_count, const I& subsectors_count, const I& x, const I& x_offset, const I& divide_by);
    void insert_region_offset_x_y(const SuperRegion<I>* r, const I& r_sectors_count, const I& subregions_count);
    void insert_region_offset_y(const SuperRegion<I>* r,
                                const I& r_sectors_count,
                                const I& subregions_count,
                                const I& x,
                                const I& x_offset,
                                const I& divide_by,
                                const I& first_index,
                                const I& last_index);

  public:
    Table(){};
    Table(const IndexSet<I>& index_set_p, const T default_value_p = std::numeric_limits<T>::signaling_NaN()) : index_set_(index_set_p) {
        data.resize(index_set_.size() * index_set_.size(), default_value_p);
    };
    inline const IndexSet<I>& index_set() const { return index_set_; };
    void insert_subsectors(const std::string& name, const std::vector<std::string>& subsectors);
    void insert_subregions(const std::string& name, const std::vector<std::string>& subregions);
    const T sum(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept;
    const T basesum(const SuperSector<I>* i, const SuperRegion<I>* r, const SuperSector<I>* j, const SuperRegion<I>* s) const noexcept;
    void write_to_csv(std::ostream& outstream) const;
    void write_to_mrio(std::ostream& outstream) const;
#ifdef LIBMRIO_WITH_NETCDF
    void write_to_netcdf(const std::string& filename) const;
#endif
    void read_from_csv(std::istream& indicesstream, std::istream& datastream, const T& threshold);
    void read_from_mrio(std::istream& instream, const T& threshold);
#ifdef LIBMRIO_WITH_NETCDF
    void read_from_netcdf(const std::string& filename, const T& threshold);
#endif

    inline T& at(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) {
        assert(index_set_.at(i, r) >= 0);
        assert(index_set_.at(j, s) >= 0);
        return at(index_set_.at(i, r), index_set_.at(j, s));
    };
    inline const T& at(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const {
        assert(index_set_.at(i, r) >= 0);
        assert(index_set_.at(j, s) >= 0);
        return at(index_set_.at(i, r), index_set_.at(j, s));
    };
    inline T& operator()(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) noexcept {
        assert(index_set_.at(i, r) >= 0);
        assert(index_set_.at(j, s) >= 0);
        return (*this)(index_set_(i, r), index_set_(j, s));
    };
    inline const T& operator()(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept {
        assert(index_set_.at(i, r) >= 0);
        assert(index_set_.at(j, s) >= 0);
        return (*this)(index_set_(i, r), index_set_(j, s));
    };
    inline T& at(const I& from, const I& to) {
        assert(from >= 0);
        assert(to >= 0);
        return data.at(from * index_set_.size() + to);
    };
    inline const T& at(const I& from, const I& to) const {
        assert(from >= 0);
        assert(to >= 0);
        return data.at(from * index_set_.size() + to);
    };
    inline T& operator()(const I& from, const I& to) noexcept {
        assert(from >= 0);
        assert(to >= 0);
        assert(from * index_set_.size() + to < data.size());
        return data[from * index_set_.size() + to];
    };
    inline const T& operator()(const I& from, const I& to) const noexcept {
        assert(from >= 0);
        assert(to >= 0);
        assert(from * index_set_.size() + to < data.size());
        return data[from * index_set_.size() + to];
    };
    /**
     * @brief Returns reference to value with Sectors/Regions of a foreign Table
     *        that is a disaggregated version of this non-disaggregated Table
     *
     * @return Reference to value
     */
    inline T& base(const SuperSector<I>* i, const SuperRegion<I>* r, const SuperSector<I>* j, const SuperRegion<I>* s) noexcept {
        assert(index_set_.base(i, r) >= 0);
        assert(index_set_.base(j, s) >= 0);
        return (*this)(index_set_.base(i, r), index_set_.base(j, s));
    };
    inline const T& base(const SuperSector<I>* i, const SuperRegion<I>* r, const SuperSector<I>* j, const SuperRegion<I>* s) const noexcept {
        assert(index_set_.base(i, r) >= 0);
        assert(index_set_.base(j, s) >= 0);
        return (*this)(index_set_.base(i, r), index_set_.base(j, s));
    };
    void replace_table_from(const Table& other) { data = other.data; };
    const std::vector<T>& raw_data() const { return data; }
    void debug_out() const;  // TODO
};
}  // namespace mrio

#endif
