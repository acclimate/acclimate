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

#include "acclimate.h"

namespace acclimate {

template<class ModelVariant>
class EconomicAgent;
template<class ModelVariant>
class Sector;

template<class ModelVariant>
class Storage {
  public:
    Sector<ModelVariant>* const sector;
    EconomicAgent<ModelVariant>* const economic_agent;
    std::unique_ptr<typename ModelVariant::PurchasingManagerType> const purchasing_manager;

  private:
    Flow input_flow_I_[3] = {Flow(0.0), Flow(0.0), Flow(0.0)};
    Forcing forcing_mu_ = Forcing(1.0);
    Stock content_S_ = Stock(0.0);
    Stock initial_content_S_star_ = Stock(0.0);
    Flow initial_input_flow_I_star_ = Flow(0.0);  // == initial_used_flow_U_star_
    Flow used_flow_U_ = Flow(0.0);
    Flow desired_used_flow_U_tilde_ = Flow(0.0);

  public:
    const Stock& content_S() const {
        assertstepnot(CONSUMPTION_AND_PRODUCTION);
        return content_S_;
    }
    const Flow& used_flow_U(const EconomicAgent<ModelVariant>* const caller = nullptr) const {
#ifdef DEBUG
        if (caller != economic_agent) {
            assertstepnot(CONSUMPTION_AND_PRODUCTION);
        }
#else
        UNUSED(caller);
#endif
        return used_flow_U_;
    }
    const Flow& desired_used_flow_U_tilde(const EconomicAgent<ModelVariant>* const caller = nullptr) const {
        if (caller != economic_agent) {
            assertstepnot(CONSUMPTION_AND_PRODUCTION);
            assertstepnot(EXPECTATION);
        }
        return desired_used_flow_U_tilde_;
    }
    const Stock& initial_content_S_star() const { return initial_content_S_star_; }
    const Flow& initial_input_flow_I_star() const { return initial_input_flow_I_star_; }
    const Flow& initial_used_flow_U_star() const {
        return initial_input_flow_I_star_;  // == initial_used_flow_U_star
    }
    const Forcing& forcing_mu() const { return forcing_mu_; }

  protected:
    void calc_content_S();

  public:
    Storage(Sector<ModelVariant>* sector_p, EconomicAgent<ModelVariant>* economic_agent_p);
    void set_desired_used_flow_U_tilde(const Flow& desired_used_flow_U_tilde_p);
    void use_content_S(const Flow& used_flow_U_current);
    const Flow get_possible_use_U_hat() const;
    void push_flow_Z(const Flow& flow_Z);
    const Flow& current_input_flow_I() const;
    const Flow& last_input_flow_I() const;
    const Flow& next_input_flow_I() const;
    Ratio get_technology_coefficient_a() const;
    Ratio get_input_share_u() const;
    void add_initial_flow_Z_star(const Flow& flow_Z_star);
    bool subtract_initial_flow_Z_star(const Flow& flow_Z_star);
    void iterate_consumption_and_production();
    inline operator std::string() const { return std::string(*sector) + ":_S_->" + std::string(*economic_agent); }
};
}  // namespace acclimate

#endif
