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

#ifndef ACCLIMATE_TRANSPORTCHAINLINK_H
#define ACCLIMATE_TRANSPORTCHAINLINK_H

#include <string>
#include <vector>
#include "types.h"

namespace acclimate {

template<class ModelVariant>
class BusinessConnection;
template<class ModelVariant>
class Model;

template<class ModelVariant>
class TransportChainLink {
  protected:
    std::vector<Flow> transport_queue;
    std::vector<FlowQuantity> initial_transport_queue;
    TransportDelay pos;

  public:
    TransportDelay current_transport_delay_tau;
    const TransportDelay initial_transport_delay_tau;
#ifdef TRANSPORT
    unique_ptr<TransportChainLink<ModelVariant>> next_transport_chain_link;
#endif
    BusinessConnection<ModelVariant>* const business_connection;

  public:
#ifdef TRANSPORT
    TransportChainLink(BusinessConnection<ModelVariant>* business_connection_p,
                       unique_ptr<TransportChainLink<ModelVariant>>& next_transport_chain_link_p,
                       const TransportDelay& initial_transport_delay_tau,
                       const Flow& initial_flow_Z_star);
#else
    TransportChainLink(BusinessConnection<ModelVariant>* business_connection_p, const TransportDelay& transport_delay_tau, const Flow& initial_flow_Z_star);
#endif
    void push_flow_Z(const Flow& flow_Z, const FlowQuantity& initial_flow_Z_star);
    void set_forcing_nu(const Forcing& forcing_nu);
    const Flow get_total_flow() const;
    const Flow get_disequilibrium() const;
    FloatType get_stddeviation() const;
    const FlowQuantity get_flow_deficit() const;

    inline Model<ModelVariant>* model() const { return business_connection->model(); }
    inline std::string id() const {
#ifdef TRANSPORT
        const TransportChainLink<ModelVariant>* link = this;
        int index = 0;
        while (link->next_transport_chain_link) {
            link = link->next_transport_chain_link.get();
            ++index;
        }
        return business_connection->seller->id() + "-" + std::to_string(index) + "->" + business_connection->buyer->storage->economic_agent->id();
#else
        return business_connection->seller->id() + "->" + business_connection->buyer->storage->economic_agent->id();
#endif
    }
};
}  // namespace acclimate

#endif
