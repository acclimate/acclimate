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

#include "acclimate.h"

namespace acclimate {

class Model;
class TransportChainLink;

class GeoEntity {
  public:
    enum class Type { LOCATION, CONNECTION };

  protected:
    Type type_m;
    non_owning_ptr<Model> model_m;

  public:
    const TransportDelay delay;
    non_owning_vector<TransportChainLink> transport_chain_links;

  public:
    GeoEntity(Model* model_p, TransportDelay delay_p, Type type_p);
    virtual ~GeoEntity();
    Type type() const { return type_m; }
    void set_forcing_nu(Forcing forcing_nu_p);

    virtual GeoConnection* as_connection() { throw log::error(this, "Not a connection"); }
    virtual const GeoConnection* as_connection() const { throw log::error(this, "Not a connection"); }

    virtual GeoLocation* as_location() { throw log::error(this, "Not a location"); }
    virtual const GeoLocation* as_location() const { throw log::error(this, "Not a location"); }

    Model* model() { return model_m; }
    const Model* model() const { return model_m; }
    virtual std::string name() const = 0;
};
}  // namespace acclimate

#endif
