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

#ifndef ACCLIMATE_BUSINESSCONNECTION_H
#define ACCLIMATE_BUSINESSCONNECTION_H

#include <cstddef>
#include <memory>
#include <string>

#include "acclimate.h"
#include "openmp.h"

namespace acclimate {

class Model;
class PurchasingManager;
class SalesManager;
class TransportChainLink;

class BusinessConnection final {
  private:
    Demand last_demand_request_D_;
    Flow initial_flow_Z_star_;
    Flow last_delivery_Z_;
    Flow last_shipment_Z_;
    Price transport_costs = Price(0.0);
    Ratio demand_fulfill_history_ = Ratio(1.0);
    Time time_;
    openmp::Lock seller_business_connections_lock;
    std::unique_ptr<TransportChainLink> first_transport_link;

  public:
    non_owning_ptr<PurchasingManager> buyer;
    non_owning_ptr<SalesManager> seller;

  public:
    BusinessConnection(PurchasingManager* buyer_p, SalesManager* seller_p, const Flow& initial_flow_Z_star_p);
    ~BusinessConnection();
    const Flow& last_shipment_Z(const SalesManager* caller = nullptr) const;
    const Flow& last_delivery_Z(const SalesManager* caller = nullptr) const;
    const Demand& last_demand_request_D(const PurchasingManager* caller = nullptr) const;
    const Flow& initial_flow_Z_star() const { return initial_flow_Z_star_; }
    void initial_flow_Z_star(const Flow& new_initial_flow_Z_star);
    std::size_t get_id(const TransportChainLink* transport_chain_link) const;
    Flow get_flow_mean() const;
    FlowQuantity get_flow_deficit() const;
    Flow get_total_flow() const;
    Flow get_transport_flow() const;
    Flow get_disequilibrium() const;
    FloatType get_stddeviation() const;
    FloatType get_minimum_passage() const;
    TransportDelay get_transport_delay_tau() const;
    void push_flow_Z(const Flow& flow_Z);
    void deliver_flow_Z(const Flow& flow_Z);
    void send_demand_request_D(const Demand& demand_request_D);
    bool get_domestic() const;

    const Model* model() const;
    std::string name() const;

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return true  //
               && o.set(H::hash("initial_flow"),
                        [this]() {  //
                            return initial_flow_Z_star();
                        })
               && o.set(H::hash("demand_request"),
                        [this]() {  //
                            return last_demand_request_D();
                        })
               && o.set(H::hash("flow_deficit"),
                        [this]() {  //
                            return get_flow_deficit();
                        })
               && o.set(H::hash("flow_mean"),
                        [this]() {  //
                            return get_flow_mean();
                        })
               && o.set(H::hash("received_flow"),
                        [this]() {  //
                            return last_delivery_Z();
                        })
               && o.set(H::hash("sent_flow"),
                        [this]() {  //
                            return last_shipment_Z();
                        })
               && o.set(H::hash("total_flow"),
                        [this]() {  //
                            return get_total_flow();
                        })
               && o.set(H::hash("minimum_passage"),
                        [this]() {  //
                            return get_minimum_passage();
                        })
            //
            ;
    }
};
}  // namespace acclimate

#endif
