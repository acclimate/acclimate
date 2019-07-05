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

#include "model/Region.h"
#include <algorithm>
#include <cstddef>
#include <utility>
#include "model/EconomicAgent.h"
#include "model/Government.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Region<ModelVariant>::Region(Model<ModelVariant>* model_p, std::string id_p, const IntType index_p)
    : GeoLocation<ModelVariant>(model_p, 0, GeoLocation<ModelVariant>::Type::REGION, std::move(id_p)), index_m(index_p) {}

template<class ModelVariant>
void Region<ModelVariant>::add_export_Z(const Flow& export_flow_Z_p) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    export_flow_Z_lock.call([&]() { export_flow_Z_[model()->current_register()] += export_flow_Z_p; });
}

template<class ModelVariant>
void Region<ModelVariant>::add_import_Z(const Flow& import_flow_Z_p) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    import_flow_Z_lock.call([&]() { import_flow_Z_[model()->current_register()] += import_flow_Z_p; });
}

template<class ModelVariant>
void Region<ModelVariant>::add_consumption_flow_Y(const Flow& consumption_flow_Y_p) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    consumption_flow_Y_lock.call([&]() { consumption_flow_Y_[model()->current_register()] += consumption_flow_Y_p; });
}

template<class ModelVariant>
Flow Region<ModelVariant>::get_gdp() const {
    return consumption_flow_Y_[model()->current_register()] + export_flow_Z_[model()->current_register()] - import_flow_Z_[model()->current_register()];
}

template<class ModelVariant>
void Region<ModelVariant>::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    export_flow_Z_[model()->other_register()] = Flow(0.0);
    import_flow_Z_[model()->other_register()] = Flow(0.0);
    consumption_flow_Y_[model()->other_register()] = Flow(0.0);
    iterate_consumption_and_production_variant();
}

#ifdef VARIANT_BASIC
template<>
void Region<VariantBasic>::iterate_consumption_and_production_variant() {}
#endif

#ifdef VARIANT_DEMAND
template<>
void Region<VariantDemand>::iterate_consumption_and_production_variant() {}
#endif

template<class ModelVariant>
void Region<ModelVariant>::iterate_consumption_and_production_variant() {
    if (government_m) {
        government_m->iterate_consumption_and_production();
    }
}

template<class ModelVariant>
void Region<ModelVariant>::iterate_expectation() {
    assertstep(EXPECTATION);
    iterate_expectation_variant();
}

#ifdef VARIANT_BASIC
template<>
void Region<VariantBasic>::iterate_expectation_variant() {}
#endif

#ifdef VARIANT_DEMAND
template<>
void Region<VariantDemand>::iterate_expectation_variant() {}
#endif

template<class ModelVariant>
void Region<ModelVariant>::iterate_expectation_variant() {
    if (government_m) {
        government_m->iterate_expectation();
    }
}

template<class ModelVariant>
void Region<ModelVariant>::iterate_purchase() {
    assertstep(PURCHASE);
    iterate_purchase_variant();
}

#ifdef VARIANT_BASIC
template<>
void Region<VariantBasic>::iterate_purchase_variant() {}
#endif

#ifdef VARIANT_DEMAND
template<>
void Region<VariantDemand>::iterate_purchase_variant() {}
#endif

template<class ModelVariant>
void Region<ModelVariant>::iterate_purchase_variant() {
    if (government_m) {
        government_m->iterate_purchase();
    }
}

template<class ModelVariant>
void Region<ModelVariant>::iterate_investment() {
    assertstep(INVESTMENT);
    iterate_investment_variant();
}

#ifdef VARIANT_BASIC
template<>
void Region<VariantBasic>::iterate_investment_variant() {}
#endif

#ifdef VARIANT_DEMAND
template<>
void Region<VariantDemand>::iterate_investment_variant() {}
#endif

template<class ModelVariant>
void Region<ModelVariant>::iterate_investment_variant() {
    if (government_m) {
        government_m->iterate_investment();
    }
}

template<class ModelVariant>
const GeoRoute<ModelVariant>& Region<ModelVariant>::find_path_to(Region<ModelVariant>* region,
                                                                 typename Sector<ModelVariant>::TransportType transport_type) const {
    const auto& it = routes.find(std::make_pair(region->index(), transport_type));
    if (it == std::end(routes)) {
        error("No transport data from " << id() << " to " << region->id() << " via " << Sector<ModelVariant>::unmap_transport_type(transport_type));
    }
    return it->second;
}

template<class ModelVariant>
inline Region<ModelVariant>* Region<ModelVariant>::as_region() {
    return this;
}

template<class ModelVariant>
inline const Region<ModelVariant>* Region<ModelVariant>::as_region() const {
    return this;
}

template<class ModelVariant>
void Region<ModelVariant>::remove_economic_agent(EconomicAgent<ModelVariant>* economic_agent) {
    economic_agents_lock.call([&]() {
        auto it = std::find_if(economic_agents.begin(), economic_agents.end(),
                               [economic_agent](const std::unique_ptr<EconomicAgent<ModelVariant>>& it) { return it.get() == economic_agent; });
        economic_agents.erase(it);
    });
}

INSTANTIATE_BASIC(Region);
}  // namespace acclimate
