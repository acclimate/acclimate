// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_CAPACITYMANAGER_H
#define ACCLIMATE_CAPACITYMANAGER_H

#include <string>

#include "acclimate.h"

namespace acclimate {

class Firm;
class Model;

class CapacityManager final {
  private:
    Flow desired_production_ = Flow(0.0);  /** \tilde{X} */
    Flow possible_production_ = Flow(0.0); /** \hat{X} */

  public:
    non_owning_ptr<Firm> firm;
    const Ratio possible_overcapacity_ratio_beta;

  private:
    void calc_possible_and_desired_production();
    Flow get_possible_production_intern(bool consider_transport_in_production_costs, bool estimate) const;

  public:
    CapacityManager(Firm* firm_, Ratio possible_overcapacity_ratio_beta_);
    ~CapacityManager() = default;
    const Flow& desired_production() const { return desired_production_; }
    const Flow& possible_production() const { return possible_production_; }
    Ratio get_production_capacity() const;          /** p */
    Ratio get_desired_production_capacity() const;  /** \tilde{p} */
    Ratio get_possible_production_capacity() const; /** \hat{p} */
    Flow get_possible_production() const;
    Flow estimate_possible_production() const; /** \hat{X} */
    Flow calc_production();                    /** X */

    void debug_print_inputs() const;

    const Model* model() const;
    std::string name() const;
};
}  // namespace acclimate

#endif
