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

#ifndef ACCLIMATE_GOVERNMENT_H
#define ACCLIMATE_GOVERNMENT_H

#include <string>
#include <unordered_map>

#include "acclimate.h"

namespace acclimate {

class Firm;
class Model;
class Region;

class Government final {
  private:
    Value budget_m;
    std::unordered_map<Firm*, Ratio> taxed_firms;

  public:
    non_owning_ptr<Region> region;

  private:
    void collect_tax();
    void redistribute_tax();
    void impose_tax();

  public:
    explicit Government(Region* region_p);
    ~Government();
    void iterate_consumption_and_production();
    void iterate_expectation();
    void iterate_purchase();
    void iterate_investment();
    void define_tax(const std::string& sector, const Ratio& tax_ratio_p);
    const Value& budget() const { return budget_m; }

    const Model* model() const;
    std::string name() const;

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return true  //
               && o.set(H::hash("budget"),
                        [this]() {  //
                            budget();
                        })
            //
            ;
    }
};
}  // namespace acclimate

#endif
