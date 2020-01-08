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

#ifndef ACCLIMATE_CAPACITYMANAGER_H
#define ACCLIMATE_CAPACITYMANAGER_H

#include <string>

#include "types.h"

namespace acclimate {

class Firm;
class Model;

class CapacityManager {
  public:
    Firm* const firm;
    const Ratio possible_overcapacity_ratio_beta;

  private:
    Flow desired_production_X_tilde_ = Flow(0.0);
    Flow possible_production_X_hat_ = Flow(0.0);

  public:
    const Flow& desired_production_X_tilde() const { return desired_production_X_tilde_; }

    const Flow& possible_production_X_hat() const { return possible_production_X_hat_; }

  protected:
    virtual void calc_possible_and_desired_production();

  public:
    CapacityManager(Firm* firm_p, Ratio possible_overcapacity_ratio_beta_p);
    ~CapacityManager() = default;
    Ratio get_production_capacity_p() const;
    Ratio get_desired_production_capacity_p_tilde() const;
    Ratio get_possible_production_capacity_p_hat() const;
    Model* model() const;
    std::string id() const;

  private:
    Flow get_possible_production_X_hat_intern(bool consider_transport_in_production_costs, bool estimate) const;

  public:
    Flow get_possible_production_X_hat() const;
    Flow estimate_possible_production_X_hat() const;
    Flow calc_production_X();
    // DEBUG
    void print_inputs() const;
};
}  // namespace acclimate

#endif
