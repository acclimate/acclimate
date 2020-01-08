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

class BusinessConnection;
class GeoEntity;
class Model;

class TransportChainLink {
    friend class BusinessConnection;

  private:
    Forcing forcing_nu;
    FlowQuantity initial_flow_quantity;
    Flow overflow;
    std::vector<AnnotatedFlow> transport_queue;
    TransportDelay pos;
    std::unique_ptr<TransportChainLink> next_transport_chain_link;
    GeoEntity* geo_entity;

  public:
    const TransportDelay initial_transport_delay_tau;
    BusinessConnection* const business_connection;

  private:
    TransportChainLink(BusinessConnection* business_connection_p,
                       const TransportDelay& initial_transport_delay_tau,
                       const Flow& initial_flow_Z_star,
                       GeoEntity* geo_entity_p);

  public:
    ~TransportChainLink();
    void push_flow_Z(const Flow& flow_Z, const FlowQuantity& initial_flow_Z_star);
    void set_forcing_nu(Forcing forcing_nu_p);
    TransportDelay transport_delay() const { return transport_queue.size(); }
    Flow get_total_flow() const;
    FloatType get_passage() const;
    FlowQuantity get_flow_deficit() const;
    void unregister_geoentity() { geo_entity = nullptr; }
    Model* model() const;
    std::string id() const;
};
}  // namespace acclimate

#endif
