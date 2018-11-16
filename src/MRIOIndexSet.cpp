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

#include "MRIOIndexSet.h"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace mrio {

template<typename I>
void IndexSet<I>::clear() {
    sectors_map.clear();
    regions_map.clear();
    supersectors_.clear();
    superregions_.clear();
    subsectors_.clear();
    subregions_.clear();
    indices_.clear();
    size_ = 0;
    total_sectors_count_ = 0;
    total_regions_count_ = 0;
}

template<typename I>
SuperSector<I>* IndexSet<I>::add_sector(const std::string& name) {
    if (!subsectors_.empty()) {
        throw std::runtime_error("Cannot add new sector when already disaggregated");
    }
    const auto res = sectors_map.find(name);
    if (res == sectors_map.end()) {
        indices_.clear();
        auto* s = new SuperSector<I>(name, supersectors_.size(), supersectors_.size());
        supersectors_.emplace_back(s);
        sectors_map.emplace(name, s);
        total_sectors_count_++;
        return s;
    }
    return res->second->as_super();
}

template<typename I>
SuperRegion<I>* IndexSet<I>::add_region(const std::string& name) {
    if (!subregions_.empty()) {
        throw std::runtime_error("Cannot add new region when already disaggregated");
    }
    const auto res = regions_map.find(name);
    if (res == regions_map.end()) {
        indices_.clear();
        auto* r = new SuperRegion<I>(name, superregions_.size(), superregions_.size());
        superregions_.emplace_back(r);
        regions_map.emplace(name, r);
        total_regions_count_++;
        return r;
    }
    return res->second->as_super();
}

template<typename I>
void IndexSet<I>::add_index(SuperSector<I>* sector_p, SuperRegion<I>* region_p) {
    region_p->sectors_.push_back(sector_p);
    sector_p->regions_.push_back(region_p);
    size_++;
}

template<typename I>
void IndexSet<I>::add_index(const std::string& sector_name, const std::string& region_name) {
    SuperSector<I>* sector_l = add_sector(sector_name);
    SuperRegion<I>* region_l = add_region(region_name);
    if (std::find(region_l->sectors_.begin(), region_l->sectors_.end(), sector_l) != region_l->sectors_.end()) {
        throw std::runtime_error("Combination of sector and region already given");
    }
    add_index(sector_l, region_l);
}

template<typename I>
void IndexSet<I>::rebuild_indices() {
    indices_.clear();
    indices_.resize(total_sectors_count_ * total_regions_count_, -1);
    I index = 0;
    for (const auto& r : superregions_) {
        if (r->has_sub()) {
            for (const auto& sub_r : r->sub()) {
                for (const auto& s : r->sectors_) {
                    if (s->has_sub()) {
                        for (const auto& sub_s : s->sub()) {
                            indices_[*sub_s * total_regions_count_ + *sub_r] = index;
                            index++;
                        }
                    } else {
                        indices_[*s * total_regions_count_ + *sub_r] = index;
                        index++;
                    }
                }
            }
        } else {
            for (const auto& s : r->sectors_) {
                if (s->has_sub()) {
                    for (const auto& sub_s : s->sub()) {
                        indices_[*sub_s * total_regions_count_ + *r] = index;
                        index++;
                    }
                } else {
                    indices_[*s * total_regions_count_ + *r] = index;
                    index++;
                }
            }
        }
    }
}

template<typename I>
IndexSet<I>::IndexSet(const IndexSet<I>& other)
    : size_(other.size_),
      total_regions_count_(other.total_regions_count_),
      total_sectors_count_(other.total_sectors_count_),
      sectors_map(other.sectors_map),
      regions_map(other.regions_map),
      total_indices(*this),
      super_indices(*this) {
    copy_pointers(other);
}

template<typename I>
IndexSet<I>& IndexSet<I>::operator=(const IndexSet<I>& other) {
    size_ = other.size_;
    total_regions_count_ = other.total_regions_count_;
    total_sectors_count_ = other.total_sectors_count_;
    sectors_map = other.sectors_map;
    regions_map = other.regions_map;
    supersectors_.clear();
    superregions_.clear();
    subsectors_.clear();
    subregions_.clear();
    copy_pointers(other);
    return *this;
}

template<typename I>
void IndexSet<I>::copy_pointers(const IndexSet<I>& other) {
    for (I it = 0; it < other.subsectors_.size(); it++) {
        auto* n = new SubSector<I>(*other.subsectors_[it]);
        subsectors_.emplace_back(n);
        sectors_map.at(n->name) = n;
    }
    for (I it = 0; it < other.subregions_.size(); it++) {
        auto* n = new SubRegion<I>(*other.subregions_[it]);
        subregions_.emplace_back(n);
        regions_map.at(n->name) = n;
    }
    for (I it = 0; it < other.supersectors_.size(); it++) {
        auto* n = new SuperSector<I>(*other.supersectors_[it]);
        supersectors_.emplace_back(n);
        sectors_map.at(n->name) = n;
        for (I it2 = 0; it2 < n->sub_.size(); it2++) {
            n->sub_[it2] = static_cast<SubSector<I>*>(sectors_map.at(n->sub_[it2]->name));
            n->sub_[it2]->parent_ = n;
        }
    }
    for (I it = 0; it < other.superregions_.size(); it++) {
        auto* n = new SuperRegion<I>(*other.superregions_[it]);
        superregions_.emplace_back(n);
        regions_map.at(n->name) = n;
        for (I it2 = 0; it2 < n->sub_.size(); it2++) {
            n->sub_[it2] = static_cast<SubRegion<I>*>(regions_map.at(n->sub_[it2]->name));
            n->sub_[it2]->parent_ = n;
        }
        for (I it2 = 0; it2 < n->sectors_.size(); it2++) {
            n->sectors_[it2] = static_cast<SuperSector<I>*>(sectors_map.at(n->sectors_[it2]->name));
        }
    }
    for (I it = 0; it < other.supersectors_.size(); it++) {
        SuperSector<I>* n = supersectors_[it].get();
        for (I it2 = 0; it2 < n->regions_.size(); it2++) {
            n->regions_[it2] = static_cast<SuperRegion<I>*>(regions_map.at(n->regions_[it2]->name));
        }
    }
    rebuild_indices();
}

template<typename I>
void IndexSet<I>::insert_subsectors(const std::string& name, const std::vector<std::string>& newsubsectors) {
    SuperSector<I>* super = sectors_map.at(name)->as_super();
    if (!super) {
        throw std::runtime_error("Sector '" + name + "' is not a super sector");
    }
    I total_index = super->total_index_;
    I level_index = subsectors_.size();
    I subindex = 0;
    for (const auto& sub_name : newsubsectors) {
        auto* sub = new SubSector<I>(sub_name, total_index, level_index, super, subindex);
        sectors_map.emplace(sub_name, sub);
        subsectors_.emplace_back(sub);
        super->sub_.push_back(sub);
        total_index++;
        level_index++;
        subindex++;
    }
    for (auto& adjust_super : supersectors_) {
        if (adjust_super->total_index_ > super->total_index_) {
            adjust_super->total_index_ += newsubsectors.size() - 1;
            for (auto& adjust_sub : adjust_super->sub()) {
                adjust_sub->total_index_ += newsubsectors.size() - 1;
            }
        }
    }
    I total_regions_size = 0;
    for (const auto& region_l : super->regions()) {
        if (!region_l->sub().empty()) {
            total_regions_size += region_l->sub().size();
        } else {
            total_regions_size++;
        }
    }
    total_sectors_count_ += (newsubsectors.size() - 1);
    size_ += (newsubsectors.size() - 1) * total_regions_size;
    rebuild_indices();
}

template<typename I>
void IndexSet<I>::insert_subregions(const std::string& name, const std::vector<std::string>& newsubregions) {
    SuperRegion<I>* super = regions_map.at(name)->as_super();
    if (!super) {
        throw std::runtime_error("Region '" + name + "' is not a super region");
    }
    I total_index = super->total_index_;
    I level_index = subregions_.size();
    I subindex = 0;
    for (const auto& sub_name : newsubregions) {
        auto* sub = new SubRegion<I>(sub_name, total_index, level_index, super, subindex);
        regions_map.emplace(sub_name, sub);
        subregions_.emplace_back(sub);
        super->sub_.push_back(sub);
        total_index++;
        level_index++;
        subindex++;
    }
    for (auto& adjust_super : superregions_) {
        if (adjust_super->total_index_ > super->total_index_) {
            adjust_super->total_index_ += newsubregions.size() - 1;
            for (auto& adjust_sub : adjust_super->sub()) {
                adjust_sub->total_index_ += newsubregions.size() - 1;
            }
        }
    }
    I total_sectors_size = 0;
    for (const auto& sector_l : super->sectors()) {
        if (!sector_l->sub().empty()) {
            total_sectors_size += sector_l->sub().size();
        } else {
            total_sectors_size++;
        }
    }
    total_regions_count_ += (newsubregions.size() - 1);
    size_ += (newsubregions.size() - 1) * total_sectors_size;
    rebuild_indices();
}

template class IndexSet<std::size_t>;
}  // namespace mrio
