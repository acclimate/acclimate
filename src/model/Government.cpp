/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
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

#include "model/Government.h"

#include <iterator>
#include <memory>
#include <numeric>
#include <utility>

#include "acclimate.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/Region.h"
#include "model/SalesManager.h"

namespace acclimate {

Government::Government(Region* region_p) : region(region_p), budget_(0.0) {}

void Government::collect_tax() {
    debug::assertstep(this, IterationStep::EXPECTATION);
    budget_ = std::accumulate(std::begin(taxed_firms), std::end(taxed_firms), Value(0.0),
                              [this](Value v, const auto& firm) { return std::move(v) + firm.first->sales_manager->get_tax() * model()->delta_t(); });
}

void Government::redistribute_tax() { debug::assertstep(this, IterationStep::INVESTMENT); }

void Government::impose_tax() {
    debug::assertstep(this, IterationStep::EXPECTATION);
    for (const auto& ps : taxed_firms) {
        log::info(this, "Imposing tax on ", ps.first->id(), " (", ps.second, ")");
        ps.first->sales_manager->impose_tax(ps.second);
    }
}

void Government::define_tax(const std::string& sector, const Ratio& tax_ratio_p) {
    debug::assertstep(this, IterationStep::SCENARIO);
    log::info(this, "Defining tax on ", sector, ":", region->id(), " (", tax_ratio_p, ")");
    Firm* ps = model()->find_firm(sector, region->id());
    if (ps != nullptr) {
        taxed_firms[ps] = tax_ratio_p;
    }
}

void Government::iterate_consumption_and_production() { debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION); }

void Government::iterate_expectation() {
    debug::assertstep(this, IterationStep::EXPECTATION);
    collect_tax();
    impose_tax();
}

void Government::iterate_purchase() { debug::assertstep(this, IterationStep::PURCHASE); }

void Government::iterate_investment() {
    debug::assertstep(this, IterationStep::INVESTMENT);
    redistribute_tax();
}

Model* Government::model() const { return region->model(); }

std::string Government::id() const { return "GOVM:" + region->id(); }

}  // namespace acclimate
