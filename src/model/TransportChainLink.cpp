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
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
TransportChainLink<ModelVariant>::TransportChainLink(BusinessConnection<ModelVariant>* business_connection_p,
                                                     const TransportDelay& transport_delay_tau,
                                                     const Flow& initial_flow_Z_star)
    : initial_transport_delay_tau(transport_delay_tau), business_connection(business_connection_p) {
    transport_queue = std::vector<Flow>(initial_transport_delay_tau, initial_flow_Z_star);
    initial_transport_queue = std::vector<FlowQuantity>(initial_transport_delay_tau, initial_flow_Z_star.get_quantity());
    pos = 0;
}

template<class ModelVariant>
void TransportChainLink<ModelVariant>::push_flow_Z(const Flow& flow_Z, const FlowQuantity& initial_flow_Z_star) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    if (transport_queue.size() > 0) {
        Flow front_flow_Z = transport_queue[pos];
        transport_queue[pos] = flow_Z;
        initial_transport_queue[pos] = initial_flow_Z_star;
        pos = (pos + 1) % transport_queue.size();
        if (next_transport_chain_link) {
            next_transport_chain_link->push_flow_Z(front_flow_Z, initial_flow_Z_star);
        } else {
            business_connection->deliver_flow_Z(front_flow_Z);
        }
    } else {
        if (next_transport_chain_link) {
            next_transport_chain_link->push_flow_Z(flow_Z, initial_flow_Z_star);
        } else {
            business_connection->deliver_flow_Z(flow_Z);
        }
    }
}

template<class ModelVariant>
void TransportChainLink<ModelVariant>::set_forcing_nu(const Forcing& forcing_nu) {
    assertstep(SCENARIO);
    error("Not implemented yet");
    UNUSED(forcing_nu);
}

template<class ModelVariant>
Flow TransportChainLink<ModelVariant>::get_total_flow() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (int i = 0; i < transport_queue.size(); i++) {
        res += transport_queue[i];
    }
    return res;
}

template<class ModelVariant>
Flow TransportChainLink<ModelVariant>::get_disequilibrium() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (int i = 0; i < transport_queue.size(); i++) {
        res.add_possibly_negative(absdiff(transport_queue[i], initial_transport_queue[i]));
    }
    return res;
}

template<class ModelVariant>
FloatType TransportChainLink<ModelVariant>::get_stddeviation() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    FloatType res = 0.0;
    for (int i = 0; i < transport_queue.size(); i++) {
        res += to_float((absdiff(transport_queue[i], initial_transport_queue[i])).get_quantity())
               * to_float((absdiff(transport_queue[i], initial_transport_queue[i])).get_quantity());
    }
    return res;
}

template<class ModelVariant>
FlowQuantity TransportChainLink<ModelVariant>::get_flow_deficit() const {
    assertstepnot(CONSUMPTION_AND_PRODUCTION);
    FlowQuantity res = FlowQuantity(0.0);
    for (int i = 0; i < transport_queue.size(); i++) {
        res += round(initial_transport_queue[i] - transport_queue[i].get_quantity());
    }
    return round(res);
}

INSTANTIATE_BASIC(TransportChainLink);
}  // namespace acclimate
