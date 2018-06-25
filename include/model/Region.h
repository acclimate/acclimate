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

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "model/GeographicEntity.h"
#include "model/GeographicPoint.h"
#include "run.h"
#include "types.h"

namespace acclimate {

template<class ModelVariant>
class EconomicAgent;
template<class ModelVariant>
class Government;
template<class ModelVariant>
class Infrastructure;
template<class ModelVariant>
class Model;

template<class ModelVariant>
struct Path {
    Distance distance;
    Infrastructure<ModelVariant>* infrastructure;
};

template<class ModelVariant>
class Region : public GeographicEntity<ModelVariant> {
  public:
    using GeographicEntity<ModelVariant>::connections;

  protected:
    Flow export_flow_Z_[2] = {Flow(0.0), Flow(0.0)};
    OpenMPLock export_flow_Z_lock;
    Flow import_flow_Z_[2] = {Flow(0.0), Flow(0.0)};
    OpenMPLock import_flow_Z_lock;
    Flow consumption_flow_Y_[2] = {Flow(0.0), Flow(0.0)};
    OpenMPLock consumption_flow_Y_lock;
    std::unique_ptr<GeographicPoint> centroid_m;
    std::unique_ptr<Government<ModelVariant>> government_m;
    const IntType index_m;
    const std::string id_m;
    typename ModelVariant::RegionParameters parameters_m;
    OpenMPLock economic_agents_lock;
    Model<ModelVariant>* const model_m;

  public:
    std::vector<std::unique_ptr<EconomicAgent<ModelVariant>>> economic_agents;
#ifndef TRANSPORT
    std::unordered_map<const Region<ModelVariant>*, Path<ModelVariant>> paths;
#endif

  public:
    Region<ModelVariant>* as_region() override;
    const Region<ModelVariant>* as_region() const override;
    void set_centroid(GeographicPoint* centroid_p) {
        assertstep(INITIALIZATION);
        return centroid_m.reset(centroid_p);
    }
    const GeographicPoint* centroid() const { return centroid_m.get(); }
    inline const Flow& consumption_C() const {
        assertstepnot(CONSUMPTION_AND_PRODUCTION);
        return consumption_flow_Y_[model()->current_register()];
    }
    inline const Flow& import_flow_Z() const {
        assertstepnot(CONSUMPTION_AND_PRODUCTION);
        return import_flow_Z_[model()->current_register()];
    }
    inline const Flow& export_flow_Z() const {
        assertstepnot(CONSUMPTION_AND_PRODUCTION);
        return export_flow_Z_[model()->current_register()];
    }
    inline void set_government(Government<ModelVariant>* government_p) {
        assertstep(INITIALIZATION);
#ifdef DEBUG
        if (government_m) {
            error("Government already set");
        }
#endif
        government_m.reset(government_p);
    }
    inline Government<ModelVariant>* government() { return government_m.get(); }
    inline Government<ModelVariant> const* government() const { return government_m.get(); }
    inline const typename ModelVariant::RegionParameters& parameters() const { return parameters_m; }
    inline const typename ModelVariant::RegionParameters& parameters_writable() const {
        assertstep(INITIALIZATION);
        return parameters_m;
    }

  private:
    void iterate_consumption_and_production_variant();
    void iterate_expectation_variant();
    void iterate_purchase_variant();
    void iterate_investment_variant();

  public:
    Region(Model<ModelVariant>* model_p, std::string id_p, IntType index_p);
    ~Region();
    inline IntType index() const { return index_m; };
    void add_export_Z(const Flow& export_flow_Z_p);
    void add_import_Z(const Flow& import_flow_Z_p);
    void add_consumption_flow_Y(const Flow& consumption_flow_Y_p);
    Flow get_gdp() const;
    void iterate_consumption_and_production();
    void iterate_expectation();
    void iterate_purchase();
    void iterate_investment();
    const Path<ModelVariant>& find_path_to(const Region<ModelVariant>* region) const;
    void remove_economic_agent(EconomicAgent<ModelVariant>* economic_agent);
    inline Model<ModelVariant>* model() const { return model_m; }
    inline std::string id() const override { return id_m; }
};
}  // namespace acclimate

#endif
