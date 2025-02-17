// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_ECONOMICAGENT_H
#define ACCLIMATE_ECONOMICAGENT_H

#include <iterator>
#include <numeric>
#include <string>

#include "acclimate.h"

namespace acclimate {

class Consumer;
class Firm;
class Model;
class Region;
class Storage;

class EconomicAgent {
  public:
    enum class type_t { CONSUMER, FIRM };

  protected:
    Forcing forcing_ = Forcing(1.0);

  public:
    owning_vector<Storage> input_storages;
    non_owning_ptr<Region> region;
    const EconomicAgent::type_t type;
    const id_t id;

  protected:
    EconomicAgent(id_t id_, Region* region_, EconomicAgent::type_t type_);

  public:
    virtual ~EconomicAgent();
    const Forcing& forcing() const { return forcing_; }
    void set_forcing(const Forcing& forcing_p);
    bool is_firm() const { return type == EconomicAgent::type_t::FIRM; }
    bool is_consumer() const { return type == EconomicAgent::type_t::CONSUMER; }
    virtual Firm* as_firm() { throw log::error(this, "Not a firm"); }
    virtual const Firm* as_firm() const { throw log::error(this, "Not a firm"); }
    virtual Consumer* as_consumer() { throw log::error(this, "Not a consumer"); }
    virtual const Consumer* as_consumer() const { throw log::error(this, "Not a consumer"); }
    virtual void initialize() = 0;
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
                                                   [](const Demand& d, const auto& is) { return d + is->used_flow(); });
                        })
               && o.set(H::hash("demand"),
                        [this]() {  //
                            return std::accumulate(std::begin(input_storages), std::end(input_storages), Demand(0.0),
                                                   [](const Demand& d, const auto& is) { return d + is->purchasing_manager->demand(); });
                        })
               && o.set(H::hash("input_flow"),
                        [this]() {  //
                            return std::accumulate(std::begin(input_storages), std::end(input_storages), Demand(0.0),
                                                   [](const Demand& d, const auto& is) { return d + is->last_input_flow(); });
                        })
               && o.set(H::hash("storage"),
                        [this]() {  //
                            return std::accumulate(std::begin(input_storages), std::end(input_storages), Stock(0.0),
                                                   [](const Stock& s, const auto& is) { return s + is->content(); });
                        })
            //
            ;
    }
};
}  // namespace acclimate

#endif
