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
#include "model/Firm.h"
#include "model/Model.h"
#include "model/Region.h"
#include "model/SalesManagerPrices.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Government<ModelVariant>::Government(Region<ModelVariant>* region_p) : region(region_p), budget_(0.0) {}

template<class ModelVariant>
void Government<ModelVariant>::collect_tax() {
    assertstep(EXPECTATION);
    for (const auto& ps : taxed_firms) {
        budget_ += ps.first->sales_manager->get_tax() * region->model->delta_t();
    }
}

template<class ModelVariant>
void Government<ModelVariant>::redistribute_tax() {
    assertstep(INVESTMENT);
}

template<class ModelVariant>
void Government<ModelVariant>::impose_tax() {
    assertstep(EXPECTATION);
    for (const auto& ps : taxed_firms) {
        info("Imposing tax on " << ps.first->id() << " (" << ps.second << ")");
        ps.first->sales_manager->impose_tax(ps.second);
    }
}

template<class ModelVariant>
void Government<ModelVariant>::define_tax(const std::string& sector, const Ratio& tax_ratio_p) {
    assertstep(SCENARIO);
    info("Defining tax on " << sector << ":" << region->id() << " (" << tax_ratio_p << ")");
    Firm<ModelVariant>* ps = region->model->find_firm(sector, region->id());
    if (ps != nullptr) {
        taxed_firms[ps] = tax_ratio_p;
    }
}

template<class ModelVariant>
void Government<ModelVariant>::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
}

template<class ModelVariant>
void Government<ModelVariant>::iterate_expectation() {
    assertstep(EXPECTATION);
    collect_tax();
    impose_tax();
}

template<class ModelVariant>
void Government<ModelVariant>::iterate_purchase() {
    assertstep(PURCHASE);
}

template<class ModelVariant>
void Government<ModelVariant>::iterate_investment() {
    assertstep(INVESTMENT);
    redistribute_tax();
}

INSTANTIATE_PRICES(Government);
}  // namespace acclimate
