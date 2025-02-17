// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

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
    Demand last_demand_request_; /** D */
    Flow baseline_flow_;         /** Z^* */
    Flow last_delivery_;         /** Z */
    Flow last_shipment_;         /** Z */
    Time time_;
    openmp::Lock seller_business_connections_lock_;
    std::unique_ptr<TransportChainLink> first_transport_link_;

  public:
    non_owning_ptr<PurchasingManager> buyer;
    non_owning_ptr<SalesManager> seller;

  public:
    BusinessConnection(PurchasingManager* buyer_, SalesManager* seller_, const Flow& baseline_flow);
    ~BusinessConnection();
    const Flow& last_shipment(const SalesManager* caller = nullptr) const;
    const Flow& last_delivery(const SalesManager* caller = nullptr) const;
    const Demand& last_demand_request(const PurchasingManager* caller = nullptr) const;
    const Flow& baseline_flow() const { return baseline_flow_; }
    std::size_t get_id(const TransportChainLink* transport_chain_link) const;
    Flow get_flow_mean() const;
    FlowQuantity get_flow_deficit() const;
    Flow get_total_flow() const;
    Flow get_transport_flow() const;
    Flow get_disequilibrium() const;
    FloatType get_stddeviation() const;
    FloatType get_minimum_passage() const;
    TransportDelay get_transport_delay() const;
    void push_flow(const Flow& flow);
    void deliver_flow(const Flow& flow);
    void send_demand_request(const Demand& demand_request);
    bool get_domestic() const;
    void iterate_investment();

    const Model* model() const;
    std::string name() const;

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return true  //
               && o.set(H::hash("baseline_flow"),
                        [this]() {  //
                            return baseline_flow();
                        })
               && o.set(H::hash("initial_flow"),  // deprecated
                        [this]() {                //
                            return baseline_flow();
                        })
               && o.set(H::hash("demand_request"),
                        [this]() {  //
                            return last_demand_request();
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
                            return last_delivery();
                        })
               && o.set(H::hash("sent_flow"),
                        [this]() {  //
                            return last_shipment();
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
