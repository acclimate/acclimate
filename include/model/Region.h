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

#ifndef ACCLIMATE_REGION_H
#define ACCLIMATE_REGION_H

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "acclimate.h"
#include "model/EconomicAgent.h"
#include "model/GeoLocation.h"
#include "model/GeoRoute.h"  // IWYU pragma: keep
#include "model/Government.h"
#include "model/Sector.h"
#include "openmp.h"
#include "parameters.h"

namespace acclimate {

class Model;

class Region : public GeoLocation {
    friend class Model;
    friend class ModelInitializer;

  private:
    struct route_hash {
        std::size_t operator()(const std::pair<IndexType, typename Sector::TransportType>& p) const {
            return (p.first << 3) | (static_cast<IntType>(p.second));
        }
    };

  private:
    Flow export_flow_Z_[2] = {Flow(0.0), Flow(0.0)};
    OpenMPLock export_flow_Z_lock;
    Flow import_flow_Z_[2] = {Flow(0.0), Flow(0.0)};
    OpenMPLock import_flow_Z_lock;
    Flow consumption_flow_Y_[2] = {Flow(0.0), Flow(0.0)};
    OpenMPLock consumption_flow_Y_lock;
    std::unordered_map<std::pair<IndexType, typename Sector::TransportType>, GeoRoute, route_hash> routes;
    std::unique_ptr<Government> government_m;
    const IndexType index_m;
    Parameters::RegionParameters parameters_m;
    OpenMPLock economic_agents_lock;

  public:
    using GeoLocation::connections;
    std::vector<std::unique_ptr<EconomicAgent>> economic_agents;

  private:
    Region(Model* model_p, std::string id_p, IndexType index_p);

  public:
    ~Region() override = default;
    const Flow& consumption_C() const;
    const Flow& import_flow_Z() const;
    const Flow& export_flow_Z() const;
    void set_government(Government* government_p);
    Government* government();
    Government const* government() const;
    const Parameters::RegionParameters& parameters() const { return parameters_m; }
    const Parameters::RegionParameters& parameters_writable() const;
    IndexType index() const { return index_m; }
    void add_export_Z(const Flow& export_flow_Z_p);
    void add_import_Z(const Flow& import_flow_Z_p);
    void add_consumption_flow_Y(const Flow& consumption_flow_Y_p);
    Flow get_gdp() const;
    void iterate_consumption_and_production();
    void iterate_expectation();
    void iterate_purchase();
    void iterate_investment();
    const GeoRoute& find_path_to(Region* region, typename Sector::TransportType transport_type) const;
    Region* as_region() override { return this; }
    const Region* as_region() const override { return this; }
    using GeoLocation::id;
    using GeoLocation::model;
    void remove_economic_agent(EconomicAgent* economic_agent);
};
}  // namespace acclimate

#endif
