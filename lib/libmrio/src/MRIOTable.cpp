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

#include "MRIOTable.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <tuple>
#ifdef LIBMRIO_WITH_NETCDF
#include <ncDim.h>
#include <ncFile.h>
#include <ncType.h>
#include <ncVar.h>
#endif
#include "csv-parser.h"

namespace mrio {

template<typename T, typename I>
const T Table<T, I>::sum(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s) const noexcept {
    T res = 0;
    if (i == nullptr) {
        for (auto& i_l : index_set_.supersectors()) {
            res += sum(i_l.get(), r, j, s);
        }
    } else if (i->has_sub()) {
        for (auto& i_l : i->as_super()->sub()) {
            res += sum(i_l, r, j, s);
        }
    } else if (r == nullptr) {
        for (auto& r_l : (i->as_super() ? i->as_super() : i->parent())->regions()) {
            res += sum(i, r_l, j, s);
        }
    } else if (r->has_sub()) {
        for (auto& r_l : r->as_super()->sub()) {
            res += sum(i, r_l, j, s);
        }
    } else if (j == nullptr) {
        for (auto& j_l : index_set_.supersectors()) {
            res += sum(i, r, j_l.get(), s);
        }
    } else if (j->has_sub()) {
        for (auto& j_l : j->as_super()->sub()) {
            res += sum(i, r, j_l, s);
        }
    } else if (s == nullptr) {
        for (auto& s_l : (j->as_super() ? j->as_super() : j->parent())->regions()) {
            res += sum(i, r, j, s_l);
        }
    } else if (s->has_sub()) {
        for (auto& s_l : s->as_super()->sub()) {
            res += sum(i, r, j, s_l);
        }
    } else {
        return (*this)(i, r, j, s);
    }
    return res;
}

template<typename T, typename I>
const T Table<T, I>::basesum(const SuperSector<I>* i, const SuperRegion<I>* r, const SuperSector<I>* j, const SuperRegion<I>* s) const noexcept {
    T res = 0;
    if (i == nullptr) {
        for (auto& i_l : index_set_.supersectors()) {
            res += basesum(i_l.get(), r, j, s);
        }
    } else if (r == nullptr) {
        for (auto& r_l : i->regions()) {
            res += basesum(i, r_l, j, s);
        }
    } else if (j == nullptr) {
        for (auto& j_l : index_set_.supersectors()) {
            res += basesum(i, r, j_l.get(), s);
        }
    } else if (s == nullptr) {
        for (auto& s_l : j->regions()) {
            res += basesum(i, r, j, s_l);
        }
    } else {
        return this->base(i, r, j, s);
    }
    return res;
}

template<typename T, typename I>
void Table<T, I>::read_indices_from_csv(std::istream& indicesstream) {
    try {
        csv::Parser parser(indicesstream);
        do {
            const std::tuple<std::string, std::string> c = parser.read<std::string, std::string>();
            index_set_.add_index(std::get<1>(c), std::get<0>(c));
        } while (parser.next_row());
        index_set_.rebuild_indices();
    } catch (const csv::parser_exception& ex) {
        std::stringstream s;
        s << ex.what();
        s << " (line " << ex.row << " col " << ex.col << ")";
        throw std::runtime_error(s.str());
    }
}

template<typename T, typename I>
void Table<T, I>::read_data_from_csv(std::istream& datastream, const T& threshold) {
    I l = 0;
    try {
        csv::Parser parser(datastream);
        auto d = data.begin();
        for (l = 0; l < index_set_.size(); l++) {
            if (l == std::numeric_limits<I>::max()) {
                throw std::runtime_error("Too many rows");
            }
            for (I c = 0; c < index_set_.size(); c++) {
                if (c == std::numeric_limits<I>::max()) {
                    throw std::runtime_error("Too many columns");
                }
                T flow = parser.read<T>();
                if (flow > threshold) {
                    *d = flow;
                }
                d++;
                parser.next_col();
            }
            parser.next_row();
        }
    } catch (const csv::parser_exception& ex) {
        std::stringstream s;
        s << ex.what();
        s << " (line " << ex.row << " col " << ex.col << ")";
        throw std::runtime_error(s.str());
    }
}

template<typename T, typename I>
void Table<T, I>::read_from_csv(std::istream& indicesstream, std::istream& datastream, const T& threshold) {
    read_indices_from_csv(indicesstream);
    data.resize(index_set_.size() * index_set_.size(), 0);
    read_data_from_csv(datastream, threshold);
}

template<typename T, typename I>
void Table<T, I>::write_to_csv(std::ostream& outstream) const {
    debug_out();
    for (const auto& row : index_set_.total_indices) {
        for (const auto& col : index_set_.total_indices) {
            outstream << at(row.index, col.index) << ",";
        }
        outstream.seekp(-1, std::ios_base::end);
        outstream << '\n';
    }
    outstream << std::flush;
    outstream.seekp(-1, std::ios_base::end);
}

#ifdef LIBMRIO_WITH_NETCDF
template<typename T, typename I>
void Table<T, I>::read_from_netcdf(const std::string& filename, const T& threshold) {
    netCDF::NcFile file(filename, netCDF::NcFile::read);

    std::size_t sectors_count = file.getDim("sector").getSize();
    {
        netCDF::NcVar sectors_var = file.getVar("sector");
        std::vector<const char*> sectors_val(sectors_count);
        sectors_var.getVar(&sectors_val[0]);
        for (const auto& sector : sectors_val) {
            index_set_.add_sector(sector);
        }
    }

    std::size_t regions_count = file.getDim("region").getSize();
    {
        netCDF::NcVar regions_var = file.getVar("region");
        std::vector<const char*> regions_val(regions_count);
        regions_var.getVar(&regions_val[0]);
        for (const auto& region : regions_val) {
            index_set_.add_region(region);
        }
    }

    netCDF::NcDim index_dim = file.getDim("index");
    if (index_dim.isNull()) {
        data.resize(regions_count * sectors_count * regions_count * sectors_count);
        if (file.getVar("flows").getDims()[0].getName() == "sector") {
            std::vector<T> data_l(regions_count * sectors_count * regions_count * sectors_count);
            file.getVar("flows").getVar(&data_l[0]);
            auto d = data.begin();
            for (const auto& region_from : index_set_.superregions()) {
                for (const auto& sector_from : index_set_.supersectors()) {
                    index_set_.add_index(sector_from.get(), region_from.get());
                    for (const auto& region_to : index_set_.superregions()) {
                        for (const auto& sector_to : index_set_.supersectors()) {
                            const T& d_l = data_l[((*sector_from * regions_count + *region_from) * sectors_count + *sector_to) * regions_count + *region_to];
                            if (d_l > threshold) {
                                *d = d_l;
                            } else {
                                *d = 0;
                            }
                            d++;
                        }
                    }
                }
            }
        } else {
            for (const auto& sector : index_set_.supersectors()) {
                for (const auto& region : index_set_.superregions()) {
                    index_set_.add_index(sector.get(), region.get());
                }
            }
            file.getVar("flows").getVar(&data[0]);
            for (auto& d : data) {
                if (d <= threshold) {
                    d = 0;
                }
            }
        }
    } else {
        std::size_t index_size = index_dim.getSize();
        netCDF::NcVar index_sector_var = file.getVar("index_sector");
        std::vector<std::uint32_t> index_sector_val(index_size);
        index_sector_var.getVar(&index_sector_val[0]);
        netCDF::NcVar index_region_var = file.getVar("index_region");
        std::vector<std::uint32_t> index_region_val(index_size);
        index_region_var.getVar(&index_region_val[0]);
        for (unsigned int i = 0; i < index_size; ++i) {
            index_set_.add_index(index_set_.supersectors()[index_sector_val[i]].get(), index_set_.superregions()[index_region_val[i]].get());
        }
        data.resize(index_size * index_size);
        file.getVar("flows").getVar(&data[0]);
        for (auto& d : data) {
            if (d <= threshold) {
                d = 0;
            }
        }
    }
    index_set_.rebuild_indices();
}
#endif

#ifdef LIBMRIO_WITH_NETCDF
template<typename T, typename I>
void Table<T, I>::write_to_netcdf(const std::string& filename) const {
    debug_out();
    netCDF::NcFile file(filename, netCDF::NcFile::replace, netCDF::NcFile::nc4);

    netCDF::NcDim sectors_dim = file.addDim("sector", index_set_.total_sectors_count());
    {
        netCDF::NcVar sectors_var = file.addVar("sector", netCDF::NcType::nc_STRING, {sectors_dim});
        I i = 0;
        for (const auto& sector : index_set_.supersectors()) {
            if (sector->has_sub()) {
                for (const auto& subsector : sector->sub()) {
                    sectors_var.putVar({i}, subsector->name);
                    ++i;
                }
            } else {
                sectors_var.putVar({i}, sector->name);
                ++i;
            }
        }
    }

    netCDF::NcDim regions_dim = file.addDim("region", index_set_.total_regions_count());
    {
        netCDF::NcVar regions_var = file.addVar("region", netCDF::NcType::nc_STRING, {regions_dim});
        I i = 0;
        for (const auto& region : index_set_.superregions()) {
            if (region->has_sub()) {
                for (const auto& subregion : region->sub()) {
                    regions_var.putVar({i}, subregion->name);
                    ++i;
                }
            } else {
                regions_var.putVar({i}, region->name);
                ++i;
            }
        }
    }

    netCDF::NcDim index_dim = file.addDim("index", index_set_.size());
    {
        netCDF::NcVar index_sector_var = file.addVar("index_sector", netCDF::NcType::nc_UINT, {index_dim});
        netCDF::NcVar index_region_var = file.addVar("index_region", netCDF::NcType::nc_UINT, {index_dim});
        for (const auto& index : index_set_.total_indices) {
            index_sector_var.putVar({index.index}, static_cast<unsigned int>(index.sector->total_index()));
            index_region_var.putVar({index.index}, static_cast<unsigned int>(index.region->total_index()));
        }
    }
    netCDF::NcVar flows_var = file.addVar("flows", netCDF::NcType::nc_FLOAT, {index_dim, index_dim});
    flows_var.setCompression(false, true, 7);
    flows_var.setFill<T>(true, std::numeric_limits<T>::quiet_NaN());
    flows_var.putVar(&data[0]);
}
#endif

template<typename T, typename I>
void Table<T, I>::insert_sector_offset_x_y(const SuperSector<I>* i, const I& i_regions_count, const I& subsectors_count) {
    auto region = i->regions().rbegin();
    if (region != i->regions().rend()) {
        auto subregion = (*region)->sub().rbegin();
        I next;
        if (subregion == (*region)->sub().rend()) {
            next = index_set_(i, *region);
        } else {
            next = index_set_(i, *subregion);
        }
        I new_size = index_set_.size() + i_regions_count * (subsectors_count - 1);
        I x_offset = new_size;
        for (I x = index_set_.size(); x-- > 0;) {
            if (x == next) {
                x_offset -= subsectors_count;
                if (subregion != (*region)->sub().rend()) {
                    ++subregion;
                }
                if (subregion == (*region)->sub().rend()) {
                    ++region;
                    if (region == i->regions().rend()) {
                        next = -1;
                    } else {
                        subregion = (*region)->sub().rbegin();
                        if (subregion == (*region)->sub().rend()) {
                            next = index_set_(i, *region);
                        } else {
                            next = index_set_(i, *subregion);
                        }
                    }
                } else {
                    next = index_set_(i, *subregion);
                }
                for (I offset = subsectors_count; offset-- > 0;) {
                    insert_sector_offset_y(i, i_regions_count, subsectors_count, x, x_offset + offset, subsectors_count);
                }
            } else {
                --x_offset;
                insert_sector_offset_y(i, i_regions_count, subsectors_count, x, x_offset, 1);
            }
        }
    }
}

template<typename T, typename I>
void Table<T, I>::insert_sector_offset_y(
    const SuperSector<I>* i, const I& i_regions_count, const I& subsectors_count, const I& x, const I& x_offset, const I& divide_by) {
    auto region = i->regions().rbegin();
    if (region != i->regions().rend()) {
        auto subregion = (*region)->sub().rbegin();
        I next;
        if (subregion == (*region)->sub().rend()) {
            next = index_set_(i, *region);
        } else {
            next = index_set_(i, *subregion);
        }
        I new_size = index_set_.size() + i_regions_count * (subsectors_count - 1);
        I y_offset = new_size;
        for (I y = index_set_.size(); y-- > 0;) {
            if (y == next) {
                y_offset -= subsectors_count;
                if (subregion != (*region)->sub().rend()) {
                    ++subregion;
                }
                if (subregion == (*region)->sub().rend()) {
                    ++region;
                    if (region == i->regions().rend()) {
                        next = -1;
                    } else {
                        subregion = (*region)->sub().rbegin();
                        if (subregion == (*region)->sub().rend()) {
                            next = index_set_(i, *region);
                        } else {
                            next = index_set_(i, *subregion);
                        }
                    }
                } else {
                    next = index_set_(i, *subregion);
                }
                for (I offset = subsectors_count; offset-- > 0;) {
                    data[x_offset * new_size + y_offset + offset] = data[x * index_set_.size() + y] / subsectors_count / divide_by;
                }
            } else {
                --y_offset;
                data[x_offset * new_size + y_offset] = data[x * index_set_.size() + y] / divide_by;
            }
        }
    }
}

template<typename T, typename I>
void Table<T, I>::insert_region_offset_x_y(const SuperRegion<I>* r, const I& r_sectors_count, const I& subregions_count) {
    auto last_sector = r->sectors().rbegin();
    if (last_sector != r->sectors().rend()) {
        auto last_subsector = (*last_sector)->sub().rbegin();
        I last_index;
        if (last_subsector == (*last_sector)->sub().rend()) {
            last_index = index_set_(*last_sector, r);
        } else {
            last_index = index_set_(*last_subsector, r);
        }
        auto first_sector = r->sectors().begin();
        auto first_subsector = (*first_sector)->sub().begin();
        I first_index;
        if (first_subsector == (*first_sector)->sub().end()) {
            first_index = index_set_(*first_sector, r);
        } else {
            first_index = index_set_(*first_subsector, r);
        }
        I new_size = index_set_.size() + r_sectors_count * (subregions_count - 1);
        for (I x = index_set_.size(); x-- > last_index + 1;) {
            insert_region_offset_y(r, r_sectors_count, subregions_count, x, new_size + x - index_set_.size(), 1, first_index, last_index);
        }
        for (I x = last_index + 1; x-- > first_index;) {
            for (I offset = subregions_count; offset-- > 0;) {
                insert_region_offset_y(r, r_sectors_count, subregions_count, x, x + offset * r_sectors_count, subregions_count, first_index, last_index);
            }
        }
        for (I x = first_index; x-- > 0;) {
            insert_region_offset_y(r, r_sectors_count, subregions_count, x, x, 1, first_index, last_index);
        }
    }
}

template<typename T, typename I>
void Table<T, I>::insert_region_offset_y(const SuperRegion<I>* r,
                                         const I& r_sectors_count,
                                         const I& subregions_count,
                                         const I& x,
                                         const I& x_offset,
                                         const I& divide_by,
                                         const I& first_index,
                                         const I& last_index) {
    (void)(r);
    I new_size = index_set_.size() + r_sectors_count * (subregions_count - 1);
    for (I y = index_set_.size(); y-- > last_index + 1;) {
        data[x_offset * new_size + new_size + y - index_set_.size()] = data[x * index_set_.size() + y] / divide_by;
    }
    for (I y = last_index + 1; y-- > first_index;) {
        for (I offset = subregions_count; offset-- > 0;) {
            data[x_offset * new_size + y + offset * r_sectors_count] = data[x * index_set_.size() + y] / subregions_count / divide_by;
        }
    }
    for (I y = first_index; y-- > 0;) {
        data[x_offset * new_size + y] = data[x * index_set_.size() + y] / divide_by;
    }
}

template<typename T, typename I>
void Table<T, I>::debug_out() const {
#ifdef DEBUGOUT
    std::cout << "\n====\n";
    std::cout << std::setprecision(3) << std::fixed;
    for (const auto& y : index_set_.total_indices) {
        std::cout << index_set_.at(y.sector, y.region) << " " << y.sector->name << " " << (!y.sector->parent() ? "     " : y.sector->parent()->name) << " "
                  << (y.sector->parent() ? *y.sector->parent() : *y.sector) << " " << (*y.sector) << " " << y.sector->level_index() << " " << y.region->name
                  << " " << (!y.region->parent() ? "     " : y.region->parent()->name) << " " << (y.region->parent() ? *y.region->parent() : *y.region) << " "
                  << (*y.region) << " " << y.region->level_index() << "  |  ";
        for (const auto& x : index_set_.total_indices) {
            if (data[x.index * index_set_.size() + y.index] <= 0) {
                std::cout << " .   ";
            } else {
                std::cout << data[x.index * index_set_.size() + y.index];
            }
            std::cout << " ";
        }
        std::cout << "\n";
    }
    std::cout << "====\n";
#endif
}

template<typename T, typename I>
void Table<T, I>::insert_subsectors(const std::string& name, const std::vector<std::string>& subsectors) {
    const Sector<I>* sector = index_set_.sector(name);
    const SuperSector<I>* i = sector->as_super();
    if (!i) {
        throw std::runtime_error("'" + name + "' is a subsector");
    }
    if (i->has_sub()) {
        throw std::runtime_error("'" + name + "' already has subsectors");
    }
    I i_regions_count = 0;
    for (const auto& region : i->regions()) {
        if (region->has_sub()) {
            i_regions_count += region->sub().size();
        } else {
            i_regions_count++;
        }
    }
    debug_out();
    data.resize((index_set_.size() + i_regions_count * (subsectors.size() - 1)) * (index_set_.size() + i_regions_count * (subsectors.size() - 1)));
    // blowup table accordingly
    // and alter values in table (equal distribution)
    insert_sector_offset_x_y(i, i_regions_count, subsectors.size());
    // alter indices
    index_set_.insert_subsectors(name, subsectors);
    debug_out();
}

template<typename T, typename I>
void Table<T, I>::insert_subregions(const std::string& name, const std::vector<std::string>& subregions) {
    const Region<I>* region = index_set_.region(name);
    const SuperRegion<I>* r = region->as_super();
    if (!r) {
        throw std::runtime_error("'" + name + "' is a subregion");
    }
    if (r->has_sub()) {
        throw std::runtime_error("'" + name + "' already has subregions");
    }
    I r_sectors_count = 0;
    for (const auto& sector : r->sectors()) {
        if (sector->has_sub()) {
            r_sectors_count += sector->sub().size();
        } else {
            r_sectors_count++;
        }
    }
    debug_out();
    data.resize((index_set_.size() + r_sectors_count * (subregions.size() - 1)) * (index_set_.size() + r_sectors_count * (subregions.size() - 1)));
    // blowup table accordingly
    // and alter values in table (equal distribution)
    insert_region_offset_x_y(r, r_sectors_count, subregions.size());
    // alter indices
    index_set_.insert_subregions(name, subregions);
    debug_out();
}

template class Table<float, std::size_t>;
template class Table<double, std::size_t>;
template class Table<int, std::size_t>;
}  // namespace mrio
