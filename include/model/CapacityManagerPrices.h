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

#ifndef ACCLIMATE_CAPACITYMANAGERPRICES_H
#define ACCLIMATE_CAPACITYMANAGERPRICES_H

#include "model/CapacityManager.h"
#include "types.h"

namespace acclimate {

template<class ModelVariant>
class Firm;

template<class ModelVariant>
class CapacityManagerPrices : public CapacityManager<ModelVariant> {
  public:
    using CapacityManager<ModelVariant>::calc_possible_and_desired_production;
    using CapacityManager<ModelVariant>::firm;
    using CapacityManager<ModelVariant>::id;
    using CapacityManager<ModelVariant>::model;
    using CapacityManager<ModelVariant>::possible_overcapacity_ratio_beta;

  private:
    const Flow get_possible_production_X_hat_intern(bool consider_transport_in_production_costs) const;

  public:
    CapacityManagerPrices(Firm<ModelVariant>* firm_p, Ratio possible_overcapacity_ratio_beta_p);
    ~CapacityManagerPrices() override = default;
    const Flow get_possible_production_X_hat() const override;
    const Flow estimate_possible_production_X_hat() const;
    const Flow calc_production_X() override;
#ifdef DEBUG
    void print_inputs() const;
#endif
};
}  // namespace acclimate

#endif
