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

#ifndef ACCLIMATE_GEOENTITY_H
#define ACCLIMATE_GEOENTITY_H

#include <string>
#include <vector>

#include "types.h"

namespace acclimate {

class Model;
class TransportChainLink;

class GeoEntity {
  public:
    enum class Type { LOCATION, CONNECTION };

  protected:
    Type type_m;
    Model* const model_m;

  public:
    const TransportDelay delay;
    std::vector<TransportChainLink*> transport_chain_links;

  public:
    GeoEntity(Model* model_p, TransportDelay delay_p, Type type_p);
    virtual ~GeoEntity();
    Type type() const { return type_m; }
    void set_forcing_nu(Forcing forcing_nu_p);
    void remove_transport_chain_link(TransportChainLink* transport_chain_link);
    Model* model() const { return model_m; }
    virtual std::string id() const = 0;
};
}  // namespace acclimate

#endif
