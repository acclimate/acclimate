// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_TRANSPORTCHAINLINK_H
#define ACCLIMATE_TRANSPORTCHAINLINK_H

#include <memory>
#include <string>
#include <vector>

#include "acclimate.h"

namespace acclimate {

class BusinessConnection;
class GeoEntity;
class Model;

class TransportChainLink final {
    friend class BusinessConnection;

  private:
    Forcing forcing_; /** \nu */
    FlowQuantity baseline_flow_quantity_;
    Flow overflow_;
    Flow outflow_ = Flow(0.0);
    std::vector<AnnotatedFlow> transport_queue_;
    TransportDelay pos_;
    std::unique_ptr<TransportChainLink> next_transport_chain_link_;
    non_owning_ptr<GeoEntity> geo_entity_;

  public:
    const TransportDelay baseline_transport_delay;
    non_owning_ptr<BusinessConnection> business_connection;

  private:
    TransportChainLink(BusinessConnection* business_connection_p,
                       const TransportDelay& baseline_transport_delay_p,
                       const Flow& baseline_flow,
                       GeoEntity* geo_entity_p);

  public:
    ~TransportChainLink();
    void push_flow(const AnnotatedFlow& flow);
    void set_forcing(Forcing forcing);
    TransportDelay transport_delay() const { return transport_queue_.size(); }
    Flow last_outflow() const { return outflow_; }
    Flow get_total_flow() const;
    FloatType get_passage() const;
    Flow get_disequilibrium() const;
    FloatType get_stddeviation() const;
    FlowQuantity get_flow_deficit() const;
    void unregister_geoentity() { geo_entity_.invalidate(); }

    const Model* model() const;
    std::string name() const;
};
}  // namespace acclimate

#endif
