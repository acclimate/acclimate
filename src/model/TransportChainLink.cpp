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

#include "model/TransportChainLink.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <memory>
#include <numeric>
#include <utility>

#include "acclimate.h"
#include "model/BusinessConnection.h"
#include "model/EconomicAgent.h"
#include "model/GeoEntity.h"
#include "model/PurchasingManager.h"
#include "model/SalesManager.h"
#include "model/Storage.h"

namespace acclimate {

TransportChainLink::TransportChainLink(BusinessConnection* business_connection_p,
                                       const TransportDelay& transport_delay_tau,
                                       const Flow& initial_flow_Z_star,
                                       GeoEntity* geo_entity_p)
    : initial_transport_delay_tau(transport_delay_tau),
      business_connection(business_connection_p),
      geo_entity(geo_entity_p),
      overflow(0.0),
      initial_flow_quantity(initial_flow_Z_star.get_quantity()),
      transport_queue(transport_delay_tau, AnnotatedFlow(initial_flow_Z_star, initial_flow_quantity)),
      pos(0),
      forcing_nu(-1) {
    if (geo_entity != nullptr) {
        geo_entity->transport_chain_links.push_back(this);
    }
}

TransportChainLink::~TransportChainLink() {
    if (geo_entity != nullptr) {
        geo_entity->remove_transport_chain_link(this);
    }
}

FloatType TransportChainLink::get_passage() const { return forcing_nu; }

void TransportChainLink::push_flow_Z(const Flow& flow_Z, const FlowQuantity& initial_flow_Z_star) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    Flow flow_to_push(0.0);
    if (!transport_queue.empty()) {
        auto front_flow_Z = transport_queue[pos];
        transport_queue[pos] = AnnotatedFlow(flow_Z, initial_flow_Z_star);
        pos = (pos + 1) % transport_queue.size();

        if (forcing_nu < 0) {
            flow_to_push = overflow + front_flow_Z.current;
        } else {
            flow_to_push = std::min(overflow + front_flow_Z.current, Flow(forcing_nu * front_flow_Z.initial, front_flow_Z.current.get_price()));
        }
        overflow = overflow + front_flow_Z.current - flow_to_push;
        if (next_transport_chain_link) {
            next_transport_chain_link->push_flow_Z(flow_to_push, initial_flow_Z_star);
        } else {
            business_connection->deliver_flow_Z(flow_to_push);
        }
    } else {
        if (forcing_nu < 0) {
            flow_to_push = overflow + flow_Z;
        } else {
            flow_to_push = std::min(overflow + flow_Z, Flow(forcing_nu * initial_flow_Z_star, flow_Z.get_price()));
        }
        overflow = overflow + flow_Z - flow_to_push;
        if (next_transport_chain_link) {
            next_transport_chain_link->push_flow_Z(flow_to_push, initial_flow_Z_star);
        } else {
            business_connection->deliver_flow_Z(flow_to_push);
        }
    }
}

void TransportChainLink::set_forcing_nu(Forcing forcing_nu_p) {
    debug::assertstep(this, IterationStep::SCENARIO);
    forcing_nu = forcing_nu_p;
}

Flow TransportChainLink::get_total_flow() const {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return overflow
           + std::accumulate(std::begin(transport_queue), std::end(transport_queue), Flow(0.0), [](const Flow& f, const auto& q) { return f + q.current; });
}

Flow TransportChainLink::get_disequilibrium() const {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (const auto& f : transport_queue) {
        res.add_possibly_negative(absdiff(f.current, f.initial));
    }
    return res;
}

FloatType TransportChainLink::get_stddeviation() const {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return std::accumulate(std::begin(transport_queue), std::end(transport_queue), 0.0, [](FloatType v, const auto& q) {
        return v + to_float((absdiff(q.current, q.initial)).get_quantity()) * to_float((absdiff(q.current, q.initial)).get_quantity());
    });
}

FlowQuantity TransportChainLink::get_flow_deficit() const {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return round(std::accumulate(std::begin(transport_queue), std::end(transport_queue), FlowQuantity(0.0),
                                 [](FlowQuantity v, const auto& q) { return std::move(v) + round(q.initial - q.current.get_quantity()); })
                 - overflow.get_quantity());
}

Model* TransportChainLink::model() const { return business_connection->model(); }

std::string TransportChainLink::id() const {
    return (business_connection->seller != nullptr ? business_connection->seller->id() : "INVALID") + "-" + std::to_string(business_connection->get_id(this))
           + "->" + (business_connection->buyer != nullptr ? business_connection->buyer->storage->economic_agent->id() : "INVALID");
}

}  // namespace acclimate
