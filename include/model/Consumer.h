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

#ifndef ACCLIMATE_CONSUMER_H
#define ACCLIMATE_CONSUMER_H

#include "acclimate.h"
#include "model/EconomicAgent.h"

namespace acclimate {

class Region;

class Consumer final : public EconomicAgent {
  public:
    using EconomicAgent::input_storages;
    using EconomicAgent::region;

  public:
    Consumer(id_t id_p, Region* region_p, const Ratio& upper_storage_limit_omega_p, const Time& initial_storage_fill_factor_psi_p);

    Consumer* as_consumer() override { return this; };
    const Consumer* as_consumer() const override { return this; };
    void iterate_consumption_and_production() override;
    void iterate_expectation() override;
    void iterate_purchase() override;
    void iterate_investment() override;

    void debug_print_details() const override;
};
}  // namespace acclimate

#endif
