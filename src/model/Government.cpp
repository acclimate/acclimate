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

#include "model/Government.h"

#include "acclimate.h"
#include "model/Firm.h"
#include "model/GeoRoute.h"
#include "model/Model.h"
#include "model/Region.h"
#include "model/SalesManager.h"

namespace acclimate {

Government::Government(Region* region_p) : region(region_p), budget_(0.0) {}

void Government::collect_tax() {
    assertstep(EXPECTATION);
    for (const auto& ps : taxed_firms) {  // TODO use std::accumulate
        budget_ += ps.first->sales_manager->get_tax() * model()->delta_t();
    }
}

void Government::redistribute_tax() { assertstep(INVESTMENT); }

void Government::impose_tax() {
    assertstep(EXPECTATION);
    for (const auto& ps : taxed_firms) {
        info("Imposing tax on " << ps.first->id() << " (" << ps.second << ")");
        ps.first->sales_manager->impose_tax(ps.second);
    }
}

void Government::define_tax(const std::string& sector, const Ratio& tax_ratio_p) {
    assertstep(SCENARIO);
    info("Defining tax on " << sector << ":" << region->id() << " (" << tax_ratio_p << ")");
    Firm* ps = model()->find_firm(sector, region->id());
    if (ps != nullptr) {
        taxed_firms[ps] = tax_ratio_p;
    }
}

void Government::iterate_consumption_and_production() { assertstep(CONSUMPTION_AND_PRODUCTION); }

void Government::iterate_expectation() {
    assertstep(EXPECTATION);
    collect_tax();
    impose_tax();
}

void Government::iterate_purchase() { assertstep(PURCHASE); }

void Government::iterate_investment() {
    assertstep(INVESTMENT);
    redistribute_tax();
}

Model* Government::model() const { return region->model(); }

std::string Government::id() const { return "GOVM:" + region->id(); }

}  // namespace acclimate
