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

#ifndef ACCLIMATE_INFRASTRUCTURE_H
#define ACCLIMATE_INFRASTRUCTURE_H

#include <string>
#include <vector>
#include "model/GeographicEntity.h"
#include "types.h"

namespace acclimate {

template<class ModelVariant>
class TransportChainLink;

template<class ModelVariant>
class Infrastructure : public GeographicEntity<ModelVariant> {
  public:
    const Distance distance;
    std::vector<TransportChainLink<ModelVariant>*> transport_chain_links;

  protected:
    Forcing forcing_nu;

  public:
    explicit Infrastructure(const Distance& distance_p);
    Infrastructure<ModelVariant>* as_infrastructure() override;
    const Infrastructure<ModelVariant>* as_infrastructure() const override;
    void set_forcing_nu(const Forcing& forcing_nu_p);
    void remove_transport_chain_link(TransportChainLink<ModelVariant>* transport_chain_link);
    inline std::string id() const override { return "INFRASTRUCTURE"; }
};
}  // namespace acclimate

#endif
