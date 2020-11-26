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
#include <unordered_map>
#include <utility>

#include "acclimate.h"
#include "model/GeoLocation.h"
#include "model/GeoRoute.h"  // IWYU pragma: keep
#include "model/Sector.h"
#include "openmp.h"
#include "parameters.h"

namespace acclimate {

class EconomicAgent;
class Government;
class Model;

class Region final : public GeoLocation {
    friend class ModelInitializer;

  private:
    struct route_hash {
        std::size_t operator()(const std::pair<IndexType, Sector::transport_type_t>& p) const { return (p.first << 3) | (static_cast<IntType>(p.second)); }
    };

  private:
    Flow export_flow_Z_[2] = {Flow(0.0), Flow(0.0)};
    openmp::Lock export_flow_Z_lock;
    Flow import_flow_Z_[2] = {Flow(0.0), Flow(0.0)};
    openmp::Lock import_flow_Z_lock;
    Flow consumption_flow_Y_[2] = {Flow(0.0), Flow(0.0)};
    openmp::Lock consumption_flow_Y_lock;
    std::unordered_map<std::pair<IndexType, Sector::transport_type_t>, GeoRoute, route_hash> routes;  // TODO improve
    std::unique_ptr<Government> government_m;
    Parameters::RegionParameters parameters_m;
    openmp::Lock economic_agents_lock;

  public:
    non_owning_vector<EconomicAgent> economic_agents;

  public:
    Region(Model* model_p, id_t id_p);
    ~Region() override;
    const Flow& consumption_C() const;
    const Flow& import_flow_Z() const;
    const Flow& export_flow_Z() const;
    void set_government(Government* government_p);
    Government* government();
    const Government* government() const;
    const Parameters::RegionParameters& parameters() const { return parameters_m; }
    const Parameters::RegionParameters& parameters_writable() const;
    void add_export_Z(const Flow& export_flow_Z_p);
    void add_import_Z(const Flow& import_flow_Z_p);
    void add_consumption_flow_Y(const Flow& consumption_flow_Y_p);
    Flow get_gdp() const;
    void iterate_consumption_and_production();
    void iterate_expectation();
    void iterate_purchase();
    void iterate_investment();
    GeoRoute& find_path_to(Region* region, Sector::transport_type_t transport_type);
    Region* as_region() override { return this; }
    const Region* as_region() const override { return this; }

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return true  //
               && o.set(H::hash("import"),
                        [this]() {  //
                            return import_flow_Z();
                        })
               && o.set(H::hash("export"),
                        [this]() {  //
                            return export_flow_Z();
                        })
               && o.set(H::hash("consumption"),
                        [this]() {  //
                            return consumption_C();
                        })
               && o.set(H::hash("gdp"),
                        [this]() {  //
                            return get_gdp();
                        })
            //
            ;
    }
};
}  // namespace acclimate

#endif
