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

#include "model/Infrastructure.h"
#include <algorithm>
#include <cstddef>
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Infrastructure<ModelVariant>::Infrastructure(const Distance& distance_p)
    : GeographicEntity<ModelVariant>(GeographicEntity<ModelVariant>::Type::INFRASTRUCTURE), distance(distance_p) {
    forcing_nu = 1;
}

template<class ModelVariant>
void Infrastructure<ModelVariant>::set_forcing_nu(const Forcing& forcing_nu_p) {
    for (std::size_t i = 0; i < transport_chain_links.size(); ++i) {
        transport_chain_links[i]->set_forcing_nu(forcing_nu_p);
    }
    forcing_nu = forcing_nu_p;
}

template<class ModelVariant>
inline Infrastructure<ModelVariant>* Infrastructure<ModelVariant>::as_infrastructure() {
    return this;
}

template<class ModelVariant>
inline const Infrastructure<ModelVariant>* Infrastructure<ModelVariant>::as_infrastructure() const {
    return this;
}

template<class ModelVariant>
void Infrastructure<ModelVariant>::remove_transport_chain_link(TransportChainLink<ModelVariant>* transport_chain_link) {
    auto it = std::find_if(transport_chain_links.begin(), transport_chain_links.end(),
                           [transport_chain_link](const TransportChainLink<ModelVariant>* it) { return it == transport_chain_link; });
    transport_chain_links.erase(it);
}

INSTANTIATE_BASIC(Infrastructure);
}  // namespace acclimate
