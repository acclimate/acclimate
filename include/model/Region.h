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

#include <unordered_map>
#include "model/EconomicAgent.h"
#include "model/GeoLocation.h"
#include "model/GeoRoute.h"
#include "model/Government.h"

namespace acclimate {

template<class ModelVariant>
class Model;
template<class ModelVariant>
class Sector;

template<class ModelVariant>
class Region : public GeoLocation<ModelVariant> {
    friend class Model<ModelVariant>;

  private:
    void iterate_consumption_and_production_variant();
    void iterate_expectation_variant();
    void iterate_purchase_variant();
    void iterate_investment_variant();

  protected:
    Flow export_flow_Z_[2] = {Flow(0.0), Flow(0.0)};
    Flow import_flow_Z_[2] = {Flow(0.0), Flow(0.0)};
    Flow consumption_flow_Y_[2] = {Flow(0.0), Flow(0.0)};
    std::unique_ptr<Government<ModelVariant>> government_;
    const IntType index_;
    typename ModelVariant::RegionParameters parameters_;

    struct route_hash {
        std::size_t operator()(const std::pair<IntType, typename Sector<ModelVariant>::TransportType>& p) const {
            return (p.first << 3) | (static_cast<IntType>(p.second));
        }
    };
    Region(Model<ModelVariant>* model_p, std::string name_p, const IntType index_p);

  public:
    Model<ModelVariant>* const model;
    std::vector<std::unique_ptr<EconomicAgent<ModelVariant>>> economic_agents;
    std::unordered_map<std::pair<IntType, typename Sector<ModelVariant>::TransportType>, GeoRoute<ModelVariant>, route_hash> routes;

    Region<ModelVariant>* as_region() override;
    const Region<ModelVariant>* as_region() const override;
    inline const Flow& consumption_C() const {
        assertstepnot(CONSUMPTION_AND_PRODUCTION);
        return consumption_flow_Y_[model->current_register()];
    };
    inline const Flow& import_flow_Z() const {
        assertstepnot(CONSUMPTION_AND_PRODUCTION);
        return import_flow_Z_[model->current_register()];
    };
    inline const Flow& export_flow_Z() const {
        assertstepnot(CONSUMPTION_AND_PRODUCTION);
        return export_flow_Z_[model->current_register()];
    };
    inline void set_government(Government<ModelVariant>* government_p) {
        assertstep(INITIALIZATION);
#ifdef DEBUG
        if (government_) {
            error("Government already set");
        }
#endif
        government_.reset(government_p);
    };
    inline Government<ModelVariant>* government() { return government_.get(); };
    inline Government<ModelVariant> const* government() const { return government_.get(); };
    inline const typename ModelVariant::RegionParameters& parameters() const { return parameters_; };
    inline const typename ModelVariant::RegionParameters& parameters_writable() const {
        assertstep(INITIALIZATION);
        return parameters_;
    };
    inline IntType index() const { return index_; };
    void add_export_Z(const Flow& export_flow_Z_p);
    void add_import_Z(const Flow& import_flow_Z_p);
    void add_consumption_flow_Y(const Flow& consumption_flow_Y_p);
    const Flow get_gdp() const;
    void iterate_consumption_and_production();
    void iterate_expectation();
    void iterate_purchase();
    void iterate_investment();
    const GeoRoute<ModelVariant>& find_path_to(Region<ModelVariant>* region, typename Sector<ModelVariant>::TransportType transport_type) const;
    void remove_economic_agent(EconomicAgent<ModelVariant>* economic_agent);
};
}  // namespace acclimate

#endif
