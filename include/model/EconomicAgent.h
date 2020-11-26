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

#ifndef ACCLIMATE_ECONOMICAGENT_H
#define ACCLIMATE_ECONOMICAGENT_H

#include <memory>
#include <string>
#include <vector>

#include "acclimate.h"
#include "model/Storage.h"
#include "parameters.h"

namespace acclimate {

class Consumer;
class Firm;
class Model;
class Region;
class Sector;

class EconomicAgent {
  public:
    enum class Type { CONSUMER, FIRM };

  private:
    Parameters::AgentParameters parameters_;

  protected:
    Forcing forcing_m = Forcing(1.0);

  public:
    const Type type;
    owning_vector<Storage> input_storages;
    non_owning_ptr<Region> region;
    const id_t id;

  protected:
    EconomicAgent(id_t id_p, Region* region_p, EconomicAgent::type_t type_p);

  public:
    virtual ~EconomicAgent();
    const Parameters::AgentParameters& parameters() const { return parameters_; }
    Parameters::AgentParameters const& parameters_writable() const;
    const Forcing& forcing() const { return forcing_m; }
    void set_forcing(const Forcing& forcing_p);
    virtual Firm* as_firm();
    virtual const Firm* as_firm() const;
    virtual Consumer* as_consumer();
    virtual const Consumer* as_consumer() const;
    bool is_firm() const { return type == Type::FIRM; }
    bool is_consumer() const { return type == Type::CONSUMER; }
    virtual void iterate_consumption_and_production() = 0;
    virtual void iterate_expectation() = 0;
    virtual void iterate_purchase() = 0;
    virtual void iterate_investment() = 0;

    virtual void debug_print_details() const = 0;

    Model* model();
    const Model* model() const;
    std::string name() const { return id.name; }

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return true  //
               && o.set(H::hash("business_connections"),
                        [this]() {  //
                            return std::accumulate(std::begin(input_storages), std::end(input_storages), 0,
                                                   [](int v, const auto& is) { return v + is->purchasing_manager->business_connections.size(); });
                        })
               && o.set(H::hash("consumption"),
                        [this]() {  //
                            return std::accumulate(std::begin(input_storages), std::end(input_storages), Demand(0.0),
                                                   [](const Demand& d, const auto& is) { return d + is->used_flow_U(); });
                        })
               && o.set(H::hash("demand"),
                        [this]() {  //
                            return std::accumulate(std::begin(input_storages), std::end(input_storages), Demand(0.0),
                                                   [](const Demand& d, const auto& is) { return d + is->purchasing_manager->demand_D(); });
                        })
               && o.set(H::hash("input_flow"),
                        [this]() {  //
                            return std::accumulate(std::begin(input_storages), std::end(input_storages), Demand(0.0),
                                                   [](const Demand& d, const auto& is) { return d + is->last_input_flow_I(); });
                        })
               && o.set(H::hash("storage"),
                        [this]() {  //
                            return std::accumulate(std::begin(input_storages), std::end(input_storages), Stock(0.0),
                                                   [](const Stock& s, const auto& is) { return s + is->content_S(); });
                        })
            //
            ;
    }
};
}  // namespace acclimate

#endif
