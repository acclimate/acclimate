/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
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
    non_owning_ptr<Model> model_m;

  public:
    const TransportDelay delay;
    const GeoEntity::type_t entity_type;
    non_owning_vector<TransportChainLink> transport_chain_links;

  public:
    GeoEntity(Model* model_p, TransportDelay delay_p, GeoEntity::type_t type_p);
    virtual ~GeoEntity();
    void set_forcing_nu(Forcing forcing_nu_p);

    virtual GeoConnection* as_connection() { throw log::error(this, "Not a connection"); }
    virtual const GeoConnection* as_connection() const { throw log::error(this, "Not a connection"); }

    virtual GeoLocation* as_location() { throw log::error(this, "Not a location"); }
    virtual const GeoLocation* as_location() const { throw log::error(this, "Not a location"); }

    Model* model() { return model_m; }
    const Model* model() const { return model_m; }
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
