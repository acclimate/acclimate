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
#include "model/BusinessConnection.h"
#include "model/GeoEntity.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
TransportChainLink<ModelVariant>::TransportChainLink(BusinessConnection<ModelVariant>* business_connection_p,
                                                     const TransportDelay& transport_delay_tau,
                                                     const Flow& initial_flow_Z_star,
                                                     GeoEntity<ModelVariant>* geo_entity_p)
    : initial_transport_delay_tau(transport_delay_tau),
      business_connection(business_connection_p),
      geo_entity(geo_entity_p),
      overflow(0.0),
      initial_flow_quantity(initial_flow_Z_star.get_quantity()),
      transport_queue(transport_delay_tau, AnnotatedFlow(initial_flow_Z_star, initial_flow_quantity)),
      pos(0),
      forcing_nu(-1) {
    if (geo_entity) {
        geo_entity->transport_chain_links.push_back(this);
    }
}

template<class ModelVariant>
TransportChainLink<ModelVariant>::~TransportChainLink() {
    if (geo_entity) {
        geo_entity->remove_transport_chain_link(this);
    }
}


template<class ModelVariant>
FloatType TransportChainLink<ModelVariant>::get_passage() const {
    return forcing_nu;
}

template<class ModelVariant>
void TransportChainLink<ModelVariant>::push_flow_Z(const Flow& flow_Z, const FlowQuantity& initial_flow_Z_star) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    Flow flow_to_push = Flow(0.0);
    if (transport_queue.size() > 0) {
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

template<class ModelVariant>
void TransportChainLink<ModelVariant>::set_forcing_nu(Forcing forcing_nu_p) {
    assertstep(SCENARIO);
    forcing_nu = forcing_nu_p;
}

template<class ModelVariant>
Flow TransportChainLink<ModelVariant>::get_total_flow() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (const auto& f : transport_queue) {
        res += f.current;
    }
    return res + overflow;
}

template<class ModelVariant>
Flow TransportChainLink<ModelVariant>::get_disequilibrium() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (const auto& f : transport_queue) {
        res.add_possibly_negative(absdiff(f.current, f.initial));
    }
    return res;
}

template<class ModelVariant>
FloatType TransportChainLink<ModelVariant>::get_stddeviation() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    FloatType res = 0.0;
    for (const auto& f : transport_queue) {
        res += to_float((absdiff(f.current, f.initial)).get_quantity()) * to_float((absdiff(f.current, f.initial)).get_quantity());
    }
    return res;
}

template<class ModelVariant>
FlowQuantity TransportChainLink<ModelVariant>::get_flow_deficit() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    FlowQuantity res = FlowQuantity(0.0);
    for (const auto& f : transport_queue) {
        res += round(f.initial - f.current.get_quantity());
    }
    //~ if (transport_queue.size() == 0) {
    //~ res  = round(-1.*overflow.get_quantity());
    //~ } else  {
    //~ for (const auto& f : transport_queue) {
    //~ res += round(f.initial - f.current.get_quantity());
    //~ }
    //~ }
    //~ return round(res - overflow.get_quantity());
    return round(res - overflow.get_quantity());
    //~ return round(res - (transport_queue->initial - overflow.get_quantity()));
}

INSTANTIATE_BASIC(TransportChainLink);
}  // namespace acclimate
