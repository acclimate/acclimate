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

#ifndef ACCLIMATE_BUSINESSCONNECTION_H
#define ACCLIMATE_BUSINESSCONNECTION_H

#include <memory>
#include <string>
#include "model/TransportChainLink.h"
#include "run.h"
#include "types.h"

namespace acclimate {

template<class ModelVariant>
class GeoRoute;
template<class ModelVariant>
struct Model;
template<class ModelVariant>
class PurchasingManager;
template<class ModelVariant>
class SalesManager;

template<class ModelVariant>
class BusinessConnection {
  protected:
    Demand last_demand_request_D_;
    Flow initial_flow_Z_star_;
    Flow last_delivery_Z_;
    Flow last_shipment_Z_;
    Price transport_costs = Price(0.0);
    Ratio demand_fulfill_history_ = Ratio(1.0);
    Time time_;
    OpenMPLock seller_business_connections_lock;
    std::unique_ptr<TransportChainLink<ModelVariant>> first_transport_link;

  public:
    typename ModelVariant::PurchasingManagerType* buyer;  // TODO encapsulate
    typename ModelVariant::SalesManagerType* seller;      // TODO encapsulate

    BusinessConnection(typename ModelVariant::PurchasingManagerType* buyer_p,
                       typename ModelVariant::SalesManagerType* seller_p,
                       const Flow& initial_flow_Z_star_p);
    inline const Time& time() const { return time_; }
    inline void time(const Time& time_p) { time_ = time_p; }
    inline const Flow& last_shipment_Z(const SalesManager<ModelVariant>* const caller = nullptr) const {
#ifdef DEBUG
        if (caller != seller) {
            assertstepnot(CONSUMPTION_AND_PRODUCTION);
        }
#else
        UNUSED(caller);
#endif
        return last_shipment_Z_;
    }
    inline const Flow& last_delivery_Z(const SalesManager<ModelVariant>* const caller = nullptr) const {
#ifdef DEBUG
        if (caller != seller) {
            assertstepnot(CONSUMPTION_AND_PRODUCTION);
        }
#else
        UNUSED(caller);
#endif
        return last_delivery_Z_;
    }
    inline const Demand& last_demand_request_D(const PurchasingManager<ModelVariant>* const caller = nullptr) const {
#ifdef DEBUG
        if (caller != buyer) {
            assertstepnot(PURCHASE);
        }
#else
        UNUSED(caller);
#endif
        return last_demand_request_D_;
    }
    inline const Flow& initial_flow_Z_star() const { return initial_flow_Z_star_; }
    inline void initial_flow_Z_star(const Flow& new_initial_flow_Z_star) {
        assertstep(INVESTMENT);
        initial_flow_Z_star_ = new_initial_flow_Z_star;
    }
    inline void invalidate_buyer() { buyer = nullptr; }
    inline void invalidate_seller() { seller = nullptr; }
    const Ratio& demand_fulfill_history() const;  // only for VariantDemand

    std::size_t get_id(const TransportChainLink<ModelVariant>* transport_chain_link) const;
    const Flow get_flow_mean() const;
    const FlowQuantity get_flow_deficit() const;
    const Flow get_total_flow() const;
    const Flow get_transport_flow() const;
    const Flow get_disequilibrium() const;
    FloatType get_stddeviation() const;
    FloatType get_minimum_passage() const;
    TransportDelay get_transport_delay_tau() const;
    void push_flow_Z(const Flow& flow_Z);
    void deliver_flow_Z(const Flow& flow_Z);
    void send_demand_request_D(const Demand& demand_request_D);
    bool get_domestic() const;

    void calc_demand_fulfill_history();  // only for VariantDemand

    inline Model<ModelVariant>* model() const { return buyer->model(); }
    inline std::string id() const { return (seller ? seller->id() : "INVALID") + "->" + (buyer ? buyer->storage->economic_agent->id() : "INVALID"); }
};
}  // namespace acclimate

#endif
