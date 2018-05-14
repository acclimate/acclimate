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

#include "model/GeoEntity.h"
#include <algorithm>
#include "model/TransportChainLink.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
GeoEntity<ModelVariant>::GeoEntity(TransportDelay delay_p, Type type_p) : delay(delay_p), type_m(type_p) {}

template<class ModelVariant>
void GeoEntity<ModelVariant>::set_forcing_nu(Forcing forcing_nu_p) {
    for (std::size_t i = 0; i < transport_chain_links.size(); i++) {
        transport_chain_links[i]->set_forcing_nu(forcing_nu_p);
        //~ debug(std::string(*transport_chain_links[i]));
    }
}
template<class ModelVariant>
GeoEntity<ModelVariant>::~GeoEntity() {
}


template<class ModelVariant>
void GeoEntity<ModelVariant>::remove_transport_chain_link(TransportChainLink<ModelVariant>* transport_chain_link) {
    auto it = std::find_if(transport_chain_links.begin(), transport_chain_links.end(),
                           [transport_chain_link](const TransportChainLink<ModelVariant>* it) { return it == transport_chain_link; });

    if (it == std::end(transport_chain_links)) {
        std::cout << "ERROR: " << this  << std::endl;
    } else {
        transport_chain_links.erase(it);
    }
}

INSTANTIATE_BASIC(GeoEntity);
}  // namespace acclimate
