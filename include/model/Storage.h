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

#ifndef ACCLIMATE_STORAGE_H
#define ACCLIMATE_STORAGE_H

#include <memory>
#include <string>

#include "parameters.h"
#include "types.h"

namespace acclimate {

class EconomicAgent;
class Model;
class PurchasingManager;
class Sector;

class Storage {
  public:
    Sector* const sector;
    EconomicAgent* const economic_agent;
    std::unique_ptr<PurchasingManager> const purchasing_manager;

  private:
    Flow input_flow_I_[3] = {Flow(0.0), Flow(0.0), Flow(0.0)};
    Forcing forcing_mu_ = Forcing(1.0);
    Stock content_S_ = Stock(0.0);
    Stock initial_content_S_star_ = Stock(0.0);
    Flow initial_input_flow_I_star_ = Flow(0.0);  // == initial_used_flow_U_star_
    Flow used_flow_U_ = Flow(0.0);
    Flow desired_used_flow_U_tilde_ = Flow(0.0);
    OpenMPLock input_flow_I_lock;
    Parameters::StorageParameters parameters_;

  public:
    const Stock& content_S() const;
    const Flow& used_flow_U(const EconomicAgent* const caller = nullptr) const;
    const Flow& desired_used_flow_U_tilde(const EconomicAgent* const caller = nullptr) const;

    const Stock& initial_content_S_star() const { return initial_content_S_star_; }

    const Flow& initial_input_flow_I_star() const { return initial_input_flow_I_star_; }

    const Flow& initial_used_flow_U_star() const {
        return initial_input_flow_I_star_;  // == initial_used_flow_U_star
    }

    const Forcing& forcing_mu() const { return forcing_mu_; }

  private:
    void calc_content_S();

  public:
    inline const Parameters::StorageParameters& parameters() const { return parameters_; }
    Parameters::StorageParameters& parameters_writable();

  public:
    Storage(Sector* sector_p, EconomicAgent* economic_agent_p);
    void set_desired_used_flow_U_tilde(const Flow& desired_used_flow_U_tilde_p);
    void use_content_S(const Flow& used_flow_U_current);
    Flow estimate_possible_use_U_hat() const;
    Flow get_possible_use_U_hat() const;
    void push_flow_Z(const Flow& flow_Z);
    const Flow& current_input_flow_I() const;
    const Flow& last_input_flow_I() const;
    const Flow& next_input_flow_I() const;
    Ratio get_technology_coefficient_a() const;
    Ratio get_input_share_u() const;
    void add_initial_flow_Z_star(const Flow& flow_Z_star);
    bool subtract_initial_flow_Z_star(const Flow& flow_Z_star);
    void iterate_consumption_and_production();
    Model* model() const;
    std::string id() const;
};
}  // namespace acclimate

#endif
