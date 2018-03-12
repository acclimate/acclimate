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
#include "model/EconomicAgent.h"
#include "model/Government.h"
#include "model/Model.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Region<ModelVariant>::Region(Model<ModelVariant>* model_p, std::string name_p, const IntType index_p)
    : GeoLocation<ModelVariant>(0, GeoLocation<ModelVariant>::Type::REGION, name_p), index_(index_p), model(model_p) {}

template<class ModelVariant>
void Region<ModelVariant>::add_export_Z(const Flow& export_flow_Z_p) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
#pragma omp critical(export_Z)
    { export_flow_Z_[model->current_register()] += export_flow_Z_p; }
}

template<class ModelVariant>
void Region<ModelVariant>::add_import_Z(const Flow& import_flow_Z_p) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
#pragma omp critical(import_Z)
    { import_flow_Z_[model->current_register()] += import_flow_Z_p; }
}

template<class ModelVariant>
void Region<ModelVariant>::add_consumption_flow_Y(const Flow& consumption_flow_Y_p) {
    assertstep(CONSUMPTION_AND_PRODUCTION);
#pragma omp critical(flow_Y)
    { consumption_flow_Y_[model->current_register()] += consumption_flow_Y_p; }
}

template<class ModelVariant>
const Flow Region<ModelVariant>::get_gdp() const {
    return consumption_flow_Y_[model->current_register()] + export_flow_Z_[model->current_register()] - import_flow_Z_[model->current_register()];
}

template<class ModelVariant>
void Region<ModelVariant>::iterate_consumption_and_production() {
    assertstep(CONSUMPTION_AND_PRODUCTION);
    export_flow_Z_[model->other_register()] = Flow(0.0);
    import_flow_Z_[model->other_register()] = Flow(0.0);
    consumption_flow_Y_[model->other_register()] = Flow(0.0);
    iterate_consumption_and_production_variant();
#ifdef SUB_PARALLELIZATION
#pragma omp parallel for default(shared) schedule(guided)
#endif
    for (std::size_t i = 0; i < economic_agents.size(); i++) {
        economic_agents[i]->iterate_consumption_and_production();
    }
}

template<>
void Region<VariantBasic>::iterate_consumption_and_production_variant() {}

template<>
void Region<VariantDemand>::iterate_consumption_and_production_variant() {}

template<class ModelVariant>
void Region<ModelVariant>::iterate_consumption_and_production_variant() {
    if (government_) {
        government_->iterate_consumption_and_production();
    }
}

template<class ModelVariant>
void Region<ModelVariant>::iterate_expectation() {
    assertstep(EXPECTATION);
    iterate_expectation_variant();
#ifdef SUB_PARALLELIZATION
#pragma omp parallel for default(shared) schedule(guided)
#endif
    for (std::size_t i = 0; i < economic_agents.size(); i++) {
        economic_agents[i]->iterate_expectation();
    }
}

template<>
void Region<VariantBasic>::iterate_expectation_variant() {}

template<>
void Region<VariantDemand>::iterate_expectation_variant() {}

template<class ModelVariant>
void Region<ModelVariant>::iterate_expectation_variant() {
    if (government_) {
        government_->iterate_expectation();
    }
}

template<class ModelVariant>
void Region<ModelVariant>::iterate_purchase() {
    assertstep(PURCHASE);
    iterate_purchase_variant();
#ifdef SUB_PARALLELIZATION
#pragma omp parallel for default(shared) schedule(guided)
#endif
    for (std::size_t i = 0; i < economic_agents.size(); i++) {
        economic_agents[i]->iterate_purchase();
    }
}

template<>
void Region<VariantBasic>::iterate_purchase_variant() {}

template<>
void Region<VariantDemand>::iterate_purchase_variant() {}

template<class ModelVariant>
void Region<ModelVariant>::iterate_purchase_variant() {
    if (government_) {
        government_->iterate_purchase();
    }
}

template<class ModelVariant>
void Region<ModelVariant>::iterate_investment() {
    assertstep(INVESTMENT);
    iterate_investment_variant();
#ifdef SUB_PARALLELIZATION
#pragma omp parallel for default(shared) schedule(guided)
#endif
    for (std::size_t i = 0; i < economic_agents.size(); i++) {
        economic_agents[i]->iterate_investment();
    }
}

template<>
void Region<VariantBasic>::iterate_investment_variant() {}

template<>
void Region<VariantDemand>::iterate_investment_variant() {}

template<class ModelVariant>
void Region<ModelVariant>::iterate_investment_variant() {
    if (government_) {
        government_->iterate_investment();
    }
}

template<class ModelVariant>
const GeoRoute<ModelVariant>& Region<ModelVariant>::find_path_to(const std::string& region_name) const {
    const auto& it = routes.find(region_name);
    if (it == std::end(routes)) {
        error("No transport data from " << std::string(*this) << " to " << region_name);
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
#pragma omp critical(economic_agents)
    {
        auto it = std::find_if(economic_agents.begin(), economic_agents.end(),
                               [economic_agent](const std::unique_ptr<EconomicAgent<ModelVariant>>& it) { return it.get() == economic_agent; });
        economic_agents.erase(it);
    }
}

INSTANTIATE_BASIC(Region);
}  // namespace acclimate
