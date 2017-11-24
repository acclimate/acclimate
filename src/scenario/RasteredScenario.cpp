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

#include "scenario/RasteredScenario.h"
#include <sstream>
#include "scenario/ExternalForcing.h"
#include "scenario/RasteredTimeData.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant, class RegionForcingType>
RasteredScenario<ModelVariant, RegionForcingType>::RasteredScenario(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p)
    : ExternalScenario<ModelVariant>(settings_p, model_p) {}

template<class ModelVariant, class RegionForcingType>
ExternalForcing* RasteredScenario<ModelVariant, RegionForcingType>::read_forcing_file(const std::string& filename, const std::string& variable_name) {
    auto result = new RasteredTimeData<FloatType>(filename, variable_name);
    if (!result->is_compatible(*iso_raster)) {
        info("ISO raster size is " << (*iso_raster / *result) << " of forcing");
        error("Forcing and ISO raster not compatible in raster resolution");
    }
    info("Proxy size is " << (*proxy / *result) << " of forcing");
    return result;
}

template<class ModelVariant, class RegionForcingType>
void RasteredScenario<ModelVariant, RegionForcingType>::internal_start() {
    const settings::SettingsNode& scenario_node = settings["scenario"];

    // open iso raster
    {
        const std::string& variable = scenario_node["isoraster"]["variable"].as<std::string>("iso");
        const std::string& filename = scenario_node["isoraster"]["file"].as<std::string>();
        iso_raster.reset(new RasteredData<int>(filename, variable));
        std::unique_ptr<netCDF::NcFile> file(new netCDF::NcFile(filename, netCDF::NcFile::read));
        const std::string& index_name = scenario_node["isoraster"]["index"].as<std::string>("index");
        netCDF::NcVar index_var = file->getVar(index_name);
        if (index_var.isNull()) {
            error("Cannot find variable '" << index_name << "' in '" << filename << "'");
        }
        const std::size_t index_size = index_var.getDims()[0].getSize();
        region_forcings.reserve(index_size);
        std::vector<const char*> index_val(index_size);
        index_var.getVar(&index_val[0]);
        for (const auto& r : index_val) {
            const auto region = model->find_region(r);
            if (region != nullptr) {
                region_forcings.emplace_back(RegionInfo{
                    region,                     // region
                    0,                          // proxy_sum
                    new_region_forcing(region)  // forcing
                });
            }
        }
    }

    // open proxy data
    {
        const std::string& variable = scenario_node["proxy"]["variable"].as<std::string>();
        const std::string& filename = scenario_node["proxy"]["file"].as<std::string>();
        proxy.reset(new RasteredData<FloatType>(filename, variable));
    }

    info("Proxy size is " << (*proxy / *iso_raster) << " of ISO raster");
}

template<class ModelVariant, class RegionForcingType>
void RasteredScenario<ModelVariant, RegionForcingType>::iterate_first_timestep() {
    FloatType total_proxy_sum = 0.0;
    FloatType total_proxy_sum_all = 0.0;
    for (const auto& x : iso_raster->x) {
        for (const auto& y : iso_raster->y) {
            const int i = iso_raster->read(x, y);
            if (i >= 0) {
                const FloatType proxy_value = proxy->read(x, y);
                total_proxy_sum_all += proxy_value;
                region_forcings[i].proxy_sum += proxy_value;
                total_proxy_sum += proxy_value;
            }
        }
    }
#ifdef DEBUG
    for (auto& r : region_forcings) {
        info(std::string(*r.region) << ": proxy sum: " << r.proxy_sum);
    }
    info("Total proxy sum: " << total_proxy_sum << " (" << total_proxy_sum_all << ")");
#endif
}

template<class ModelVariant, class RegionForcingType>
void RasteredScenario<ModelVariant, RegionForcingType>::read_forcings() {
    total_current_proxy_sum_ = 0.0;
    auto forcing_l = static_cast<RasteredTimeData<FloatType>*>(forcing.get());
    FloatType sub_cnt = *proxy / *forcing_l;
    for (const auto& x : forcing_l->x) {
        for (const auto& y : forcing_l->y) {
            const int i = iso_raster->read(x, y);
            if (i >= 0) {
                const FloatType forcing_v = forcing_l->read(x, y);
                const FloatType proxy_value = proxy->read(x, y);
                RegionInfo& region_info = region_forcings[i];
                total_current_proxy_sum_ += add_cell_forcing(x, y, proxy_value / sub_cnt / sub_cnt, forcing_v, region_info.region, region_info.forcing);
            }
        }
    }
    info("Total current proxy sum: " << total_current_proxy_sum_);
}

template<class ModelVariant, class RegionForcingType>
bool RasteredScenario<ModelVariant, RegionForcingType>::internal_iterate() {
    for (auto& r : region_forcings) {
        set_region_forcing(r.region, r.forcing, r.proxy_sum);
    }
    return true;
}

template class RasteredScenario<VariantBasic, FloatType>;
template class RasteredScenario<VariantDemand, FloatType>;
template class RasteredScenario<VariantPrices, FloatType>;
template class RasteredScenario<VariantBasic, std::vector<FloatType>>;
template class RasteredScenario<VariantDemand, std::vector<FloatType>>;
template class RasteredScenario<VariantPrices, std::vector<FloatType>>;
}  // namespace acclimate
