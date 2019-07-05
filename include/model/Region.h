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
#include "model/GeoLocation.h"
#include "model/GeoRoute.h"
#include "model/Government.h"
#include "run.h"
#include "types.h"

namespace acclimate {

template<class ModelVariant>
class EconomicAgent;
template<class ModelVariant>
class Infrastructure;
template<class ModelVariant>
class Model;
template<class ModelVariant>
class Sector;

template<class ModelVariant>
class Region : public GeoLocation<ModelVariant> {
    friend class Model<ModelVariant>;

  protected:
    Flow export_flow_Z_[2] = {Flow(0.0), Flow(0.0)};
    OpenMPLock export_flow_Z_lock;
    Flow import_flow_Z_[2] = {Flow(0.0), Flow(0.0)};
    OpenMPLock import_flow_Z_lock;
    Flow consumption_flow_Y_[2] = {Flow(0.0), Flow(0.0)};
    OpenMPLock consumption_flow_Y_lock;
    std::unique_ptr<Government<ModelVariant>> government_m;
    const IntType index_m;
    typename ModelVariant::RegionParameters parameters_m;
    OpenMPLock economic_agents_lock;

    struct route_hash {
        std::size_t operator()(const std::pair<IntType, typename Sector<ModelVariant>::TransportType>& p) const {
            return (p.first << 3) | (static_cast<IntType>(p.second));
        }
    };
    Region(Model<ModelVariant>* model_p, std::string id_p, IntType index_p);
    void iterate_consumption_and_production_variant();
    void iterate_expectation_variant();
    void iterate_purchase_variant();
    void iterate_investment_variant();

  public:
    std::vector<std::unique_ptr<EconomicAgent<ModelVariant>>> economic_agents;
    std::unordered_map<std::pair<IntType, typename Sector<ModelVariant>::TransportType>, GeoRoute<ModelVariant>, route_hash> routes;
    using GeoLocation<ModelVariant>::connections;

    using GeoLocation<ModelVariant>::id;
    using GeoLocation<ModelVariant>::model;
    Region<ModelVariant>* as_region() override;
    const Region<ModelVariant>* as_region() const override;
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
    ~Region() override = default;
    inline IntType index() const { return index_m; };
    void add_export_Z(const Flow& export_flow_Z_p);
    void add_import_Z(const Flow& import_flow_Z_p);
    void add_consumption_flow_Y(const Flow& consumption_flow_Y_p);
    Flow get_gdp() const;
    void iterate_consumption_and_production();
    void iterate_expectation();
    void iterate_purchase();
    void iterate_investment();
    const GeoRoute<ModelVariant>& find_path_to(Region<ModelVariant>* region, typename Sector<ModelVariant>::TransportType transport_type) const;
    void remove_economic_agent(EconomicAgent<ModelVariant>* economic_agent);
};
}  // namespace acclimate

#endif
