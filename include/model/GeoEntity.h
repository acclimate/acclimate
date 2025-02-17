// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_GEOENTITY_H
#define ACCLIMATE_GEOENTITY_H

#include <numeric>
#include <string>

#include "acclimate.h"

namespace acclimate {

class GeoConnection;
class GeoLocation;
class Model;
class TransportChainLink;

class GeoEntity {
  public:
    enum class type_t { LOCATION, CONNECTION };

  protected:
    non_owning_ptr<Model> model_;

  public:
    const TransportDelay delay;
    const GeoEntity::type_t entity_type;
    non_owning_vector<TransportChainLink> transport_chain_links;

  public:
    GeoEntity(Model* model, TransportDelay delay_, GeoEntity::type_t type);
    virtual ~GeoEntity();
    void set_forcing_nu(Forcing forcing_nu);

    virtual GeoConnection* as_connection() { throw log::error(this, "Not a connection"); }
    virtual const GeoConnection* as_connection() const { throw log::error(this, "Not a connection"); }

    virtual GeoLocation* as_location() { throw log::error(this, "Not a location"); }
    virtual const GeoLocation* as_location() const { throw log::error(this, "Not a location"); }

    Model* model() { return model_; }
    const Model* model() const { return model_; }
    virtual std::string name() const = 0;

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return true  //
               && o.set(H::hash("total_flow"),
                        [this]() {  //
                            return std::accumulate(
                                std::begin(transport_chain_links), std::end(transport_chain_links), Flow(0.0),
                                [](const Flow& flow, const auto& transport_chain_link) { return flow + transport_chain_link->get_total_flow(); });
                        })
               && o.set(H::hash("total_outflow"),
                        [this]() {  //
                            return std::accumulate(
                                std::begin(transport_chain_links), std::end(transport_chain_links), Flow(0.0),
                                [](const Flow& flow, const auto& transport_chain_link) { return flow + transport_chain_link->last_outflow(); });
                        })
            //
            ;
    }
};
}  // namespace acclimate

#endif
