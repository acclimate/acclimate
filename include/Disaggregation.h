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

#ifndef LIBMRIO_DISAGGREGATION_H
#define LIBMRIO_DISAGGREGATION_H

#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace csv {
class Parser;
}
namespace mrio {
template<typename I>
class IndexPart;
}
namespace mrio {
template<typename I>
class Region;
}
namespace mrio {
template<typename I>
class Sector;
}
namespace mrio {
template<typename T, typename I>
class Table;
}
namespace settings {
class SettingsNode;
}

namespace mrio {

template<typename T, typename I>
class Disaggregation {
  protected:
    enum {
        LEVEL_EQUALLY_0 = 0,
        LEVEL_POPULATION_1 = 1,
        LEVEL_GDP_SUBREGION_2 = 2,
        LEVEL_GDP_SUBSECTOR_3 = 3,
        LEVEL_GDP_SUBREGIONAL_SUBSECTOR_4 = 4,
        LEVEL_IMPORT_SUBSECTOR_5 = 5,
        LEVEL_IMPORT_SUBREGION_6 = 6,
        LEVEL_INTERREGIONAL_SUBSECTOR_INPUT_7 = 7,
        LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_8 = 8,
        LEVEL_IMPORT_SUBSECTOR_BY_REGIONAL_SECTOR_9 = 9,
        LEVEL_EXPORT_SUBREGION_10 = 10,
        LEVEL_SUBREGIONAL_SUBSECTOR_INPUT_11 = 11,
        LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_REGION_12 = 12,
        LEVEL_IMPORT_SUBREGIONAL_SUBSECTOR_13 = 13,
        LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_SUBREGION_14 = 14,
        LEVEL_PETERS1_15 = 15,
        LEVEL_PETERS2_16 = 16,
        LEVEL_PETERS3_17 = 17,
        LEVEL_EXACT_18 = 18
    };
    static const int PROXY_COUNT = LEVEL_EXACT_18 + 1;
    class ProxyData {
      protected:
        std::vector<T> data;
        int dim;
        I size[4];

      public:
        ProxyData(const I& size1) : dim(1) {
            size[0] = size1;
            data.resize(size1, std::numeric_limits<T>::quiet_NaN());
        };
        ProxyData(const I& size1, const I& size2) : dim(2) {
            size[0] = size1;
            size[1] = size2;
            data.resize(size1 * size2, std::numeric_limits<T>::quiet_NaN());
        };
        ProxyData(const I& size1, const I& size2, const I& size3) : dim(3) {
            size[0] = size1;
            size[1] = size2;
            size[2] = size3;
            data.resize(size1 * size2 * size3, std::numeric_limits<T>::quiet_NaN());
        };
        ProxyData(const I& size1, const I& size2, const I& size3, const I& size4) : dim(4) {
            size[0] = size1;
            size[1] = size2;
            size[2] = size3;
            size[3] = size4;
            data.resize(size1 * size2 * size3 * size4, std::numeric_limits<T>::quiet_NaN());
        };
        inline T& at(const IndexPart<I>* i1) { return data.at(i1->level_index()); };
        inline T& at(const IndexPart<I>* i1, const IndexPart<I>* i2) { return data.at(i1->level_index() + (i2->level_index()) * size[0]); };
        inline T& at(const IndexPart<I>* i1, const IndexPart<I>* i2, const IndexPart<I>* i3) {
            return data.at(i1->level_index() + (i2->level_index() + (i3->level_index()) * size[1]) * size[0]);
        };
        inline T& at(const IndexPart<I>* i1, const IndexPart<I>* i2, const IndexPart<I>* i3, const IndexPart<I>* i4) {
            return data.at(i1->level_index() + (i2->level_index() + (i3->level_index() + i4->level_index() * size[2]) * size[1]) * size[0]);
        };
        inline T& operator()(const IndexPart<I>* i1) { return data[i1->level_index()]; };
        inline T& operator()(const IndexPart<I>* i1, const IndexPart<I>* i2) { return data[i1->level_index() + (i2->level_index()) * size[0]]; };
        inline T& operator()(const IndexPart<I>* i1, const IndexPart<I>* i2, const IndexPart<I>* i3) {
            return data[i1->level_index() + (i2->level_index() + (i3->level_index()) * size[1]) * size[0]];
        };
        inline T& operator()(const IndexPart<I>* i1, const IndexPart<I>* i2, const IndexPart<I>* i3, const IndexPart<I>* i4) {
            return data[i1->level_index() + (i2->level_index() + (i3->level_index() + i4->level_index() * size[2]) * size[1]) * size[0]];
        };
    };

    const Table<T, I>* basetable;
    std::unique_ptr<ProxyData> proxies[PROXY_COUNT];
    std::unique_ptr<ProxyData> proxy_sums[PROXY_COUNT];

    // only needed during actual disaggregation
    std::unique_ptr<mrio::Table<T, I>> last_table;  // table in disaggregation used for accessing d-1 values
    std::unique_ptr<mrio::Table<T, I>> table;
    std::unique_ptr<mrio::Table<int, I>> quality;

    inline const Sector<I>* read_sector(csv::Parser& in);
    inline const Region<I>* read_region(csv::Parser& in);
    inline const Sector<I>* read_subsector(csv::Parser& in);
    inline const Region<I>* read_subregion(csv::Parser& in);

    void approximate(const int& d);
    void adjust(const int& d);
    void read_proxy_file(const std::string& filename, int d, I year);

  public:
    Disaggregation(const Table<T, I>* basetable_p);
    virtual ~Disaggregation(){};
    void refine();
    void initialize(const settings::SettingsNode& settings);
    const Table<T, I>& refined_table() const { return *table; }
};
}  // namespace mrio

#endif
