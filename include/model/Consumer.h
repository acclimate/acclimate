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

#ifndef ACCLIMATE_CONSUMER_H
#define ACCLIMATE_CONSUMER_H

#include "model/EconomicAgent.h"

namespace acclimate {

template<class ModelVariant>
class Region;
template<class ModelVariant>
class Scenario;

template<class ModelVariant>
class Consumer : public EconomicAgent<ModelVariant> {
  public:
    using EconomicAgent<ModelVariant>::input_storages;
    using EconomicAgent<ModelVariant>::region;

  protected:
    using EconomicAgent<ModelVariant>::forcing_;

  protected:
    void add_revenue_to_budget();
    void substract_input_costs_from_budget();
#ifdef DEBUG
    void print_details() const override;
#endif

  public:
    Consumer<ModelVariant>* as_consumer() override;
    explicit Consumer(Region<ModelVariant>* region_p);
    void iterate_consumption_and_production() override;
    void iterate_expectation() override;
    void iterate_purchase() override;
    void iterate_investment() override;
};
}  // namespace acclimate

#endif
