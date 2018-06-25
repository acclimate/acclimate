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

#ifndef ACCLIMATE_GEOGRAPHICENTITY_H
#define ACCLIMATE_GEOGRAPHICENTITY_H

#include <string>
#include <vector>

namespace acclimate {

template<class ModelVariant>
class Region;
template<class ModelVariant>
class Infrastructure;

template<class ModelVariant>
class GeographicEntity {
  public:
    enum class Type { REGION, INFRASTRUCTURE };

  public:
    std::vector<GeographicEntity<ModelVariant>*> connections;
    const Type type;
    virtual Region<ModelVariant>* as_region();
    virtual Infrastructure<ModelVariant>* as_infrastructure();
    virtual const Region<ModelVariant>* as_region() const;
    virtual const Infrastructure<ModelVariant>* as_infrastructure() const;

  protected:
    explicit GeographicEntity(const GeographicEntity<ModelVariant>::Type& type_p);
    virtual ~GeographicEntity();
    void remove_connection(const GeographicEntity<ModelVariant>* geographic_entity);
    virtual std::string id() const = 0;
};
}  // namespace acclimate

#endif
