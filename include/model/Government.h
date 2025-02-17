// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

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
    Value budget_;
    std::unordered_map<Firm*, Ratio> taxed_firms_;

  public:
    non_owning_ptr<Region> region;

  private:
    void collect_tax();
    void redistribute_tax() const;
    void impose_tax();

  public:
    explicit Government(Region* region_);
    ~Government();
    void iterate_consumption_and_production() const;
    void iterate_expectation();
    void iterate_purchase() const;
    void iterate_investment();
    void define_tax(const std::string& sector, const Ratio& tax_ratio);
    const Value& budget() const { return budget_; }

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
