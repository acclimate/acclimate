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

#include "model/BusinessConnection.h"
#include "model/EconomicAgent.h"
#include "model/GeoRoute.h"
#include "model/Model.h"
#include "model/Region.h"
#include "model/SalesManagerBasic.h"
#include "model/SalesManagerDemand.h"
#include "model/SalesManagerPrices.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "model/TransportChainLink.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
BusinessConnection<ModelVariant>::BusinessConnection(typename ModelVariant::PurchasingManagerType* buyer_p,
                                                     typename ModelVariant::SalesManagerType* seller_p,
                                                     const Flow& initial_flow_Z_star_p)
    : buyer(buyer_p),
      demand_fulfill_history_(1.0),
      initial_flow_Z_star_(initial_flow_Z_star_p),
      last_delivery_Z_(initial_flow_Z_star_p),
      last_demand_request_D_(initial_flow_Z_star_p),
      seller(seller_p),
      transport_costs(0.0),
      last_shipment_Z_(initial_flow_Z_star_p),
      time_(seller_p->firm->sector->model->time()) {
    if (seller->firm->sector->transport_type == Sector<ModelVariant>::TransportType::IMMEDIATE
        || buyer->storage->economic_agent->region == seller->firm->region) {
        first_transport_link.reset(new TransportChainLink<ModelVariant>(this, 0, initial_flow_Z_star_p, nullptr));
    } else {
        const auto& route = seller->firm->region->find_path_to(buyer->storage->economic_agent->region, seller->firm->sector->transport_type);
        assert(route.path.size() > 0);
        TransportChainLink<ModelVariant>* link;
        for (int i = 0; i < route.path.size(); ++i) {
            GeoEntity<ModelVariant>* p = route.path[i];
            auto new_link = new TransportChainLink<ModelVariant>(this, p->delay, initial_flow_Z_star_p, p);
            if (i == 0) {
                first_transport_link.reset(new_link);
            } else {
                link->next_transport_chain_link.reset(new_link);
            }
            link = new_link;
        }
    }
}

template<>
const Ratio& BusinessConnection<VariantDemand>::demand_fulfill_history() const {
    return demand_fulfill_history_;
}

template<class ModelVariant>
FloatType BusinessConnection<ModelVariant>::get_minimum_passage() const {
    TransportChainLink<ModelVariant>* link = first_transport_link.get();
    FloatType minimum_passage = 1.0;
    FloatType link_passage;
    while (link) {
        link_passage = link->get_passage();
        if (link_passage >= 0.0 && link_passage < minimum_passage) {
            minimum_passage = link_passage;
        }
        link = link->next_transport_chain_link.get();
    }
    if (minimum_passage > 1.0 || minimum_passage < 0.0) {
        minimum_passage = 1.0;
    }
    return minimum_passage;
}

template<class ModelVariant>
bool BusinessConnection<ModelVariant>::get_domestic() const {
    return (buyer->storage->economic_agent->region == seller->firm->region);
}

template<class ModelVariant>
void BusinessConnection<ModelVariant>::push_flow_Z(const Flow& flow_Z) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    last_shipment_Z_ = round(flow_Z);
    first_transport_link->push_flow_Z(last_shipment_Z_, initial_flow_Z_star_.get_quantity());
    if (!get_domestic()) {
        seller->firm->region->add_export_Z(last_shipment_Z_);
    }
}

template<class ModelVariant>
TransportDelay BusinessConnection<ModelVariant>::get_transport_delay_tau() const {
    TransportChainLink<ModelVariant>* link = first_transport_link.get();
    TransportDelay res = 0;
    while (link) {
        res += link->transport_delay();
        link = link->next_transport_chain_link.get();
    }
    return res;
}

template<class ModelVariant>
void BusinessConnection<ModelVariant>::deliver_flow_Z(const Flow& flow_Z) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    buyer->storage->push_flow_Z(flow_Z);
    last_delivery_Z_ = flow_Z;
    if (!get_domestic()) {
        buyer->storage->economic_agent->region->add_import_Z(flow_Z);
    }
}

template<class ModelVariant>
std::size_t BusinessConnection<ModelVariant>::get_id(const TransportChainLink<ModelVariant>* transport_chain_link) const {
    std::size_t id = 0;
    if (first_transport_link) {
        const TransportChainLink<ModelVariant>* link = first_transport_link.get();
        while (link->next_transport_chain_link && transport_chain_link != link->next_transport_chain_link.get()) {
            link = link->next_transport_chain_link.get();
            id++;
        }
    }
    return id;
}

template<class ModelVariant>
void BusinessConnection<ModelVariant>::send_demand_request_D(const Demand& demand_request_D) {
    assertstep(PURCHASE);
    last_demand_request_D_ = round(demand_request_D);
    seller->add_demand_request_D(last_demand_request_D_);
}

template<class ModelVariant>
const Flow BusinessConnection<ModelVariant>::get_flow_mean() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    TransportChainLink<ModelVariant>* link = first_transport_link.get();
    Flow res = last_delivery_Z_;
    TransportDelay delay = 0;
    while (link) {
        res += link->get_total_flow();
        delay += link->transport_delay();
        link = link->next_transport_chain_link.get();
    }
    return round(res / Ratio(delay));
}

template<class ModelVariant>
const FlowQuantity BusinessConnection<ModelVariant>::get_flow_deficit() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    TransportChainLink<ModelVariant>* link = first_transport_link.get();
    FlowQuantity res = initial_flow_Z_star_.get_quantity() - last_delivery_Z_.get_quantity();
    while (link) {
        res += link->get_flow_deficit();
        link = link->next_transport_chain_link.get();
    }
    return round(res);
}

template<class ModelVariant>
const Flow BusinessConnection<ModelVariant>::get_total_flow() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    TransportChainLink<ModelVariant>* link = first_transport_link.get();
    Flow res = Flow(0.0);
    while (link) {
        res += link->get_total_flow();
        link = link->next_transport_chain_link.get();
    }
    return round(res);
}

template<class ModelVariant>
const Flow BusinessConnection<ModelVariant>::get_disequilibrium() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    TransportChainLink<ModelVariant>* link = first_transport_link.get();
    Flow res = Flow(0.0);
    while (link) {
        res.add_possibly_negative(link->get_disequilibrium());
        link = link->next_transport_chain_link.get();
    }
    return round(res, true);
}

template<class ModelVariant>
FloatType BusinessConnection<ModelVariant>::get_stddeviation() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    TransportChainLink<ModelVariant>* link = first_transport_link.get();
    FloatType res = 0.0;
    while (link) {
        res += link->get_stddeviation();
        link = link->next_transport_chain_link.get();
    }
    return res;
}

template<>
void BusinessConnection<VariantDemand>::calc_demand_fulfill_history() {
    assertstep(PURCHASE);
    // Checks if and in how far demand was fulfilled by these suppliers
    Ratio demand_fulfill;
    if (last_demand_request_D_.get_quantity() > 0.0) {
        demand_fulfill = last_shipment_Z_ / last_demand_request_D_;
    } else {
        demand_fulfill = 0;
    }

    demand_fulfill_history_ = seller->firm->sector->model->parameters().history_weight * demand_fulfill_history_
                              + (1 - seller->firm->sector->model->parameters().history_weight) * demand_fulfill;

    if (demand_fulfill_history_ < 1e-2) {
        demand_fulfill_history_ = 1e-2;
        Acclimate::Run<VariantDemand>::instance()->event(EventType::DEMAND_FULFILL_HISTORY_UNDERFLOW, seller->firm, buyer->storage->economic_agent);
    }
}

INSTANTIATE_BASIC(BusinessConnection);
}  // namespace acclimate
