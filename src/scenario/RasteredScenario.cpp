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

#include <cstddef>

#include "model/Model.h"
#include "model/Region.h"
#include "run.h"
#include "scenario/RasteredTimeData.h"
#include "settingsnode.h"

namespace acclimate {

template<class RegionForcingType>
RasteredScenario<RegionForcingType>::RasteredScenario(const settings::SettingsNode& settings_p, settings::SettingsNode scenario_node_p, Model* model_p)
    : ExternalScenario(settings_p, std::move(scenario_node_p), model_p) {}

template<class RegionForcingType>
ExternalForcing* RasteredScenario<RegionForcingType>::read_forcing_file(const std::string& filename, const std::string& variable_name) {
    auto result = new RasteredTimeData<FloatType>(filename, variable_name);
    if (!result->is_compatible(*iso_raster)) {
        info("ISO raster size is " << (*iso_raster / *result) << " of forcing");
        error("Forcing and ISO raster not compatible in raster resolution");
    }
    info("Proxy size is " << (*proxy / *result) << " of forcing");
    return result;
}

template<class RegionForcingType>
void RasteredScenario<RegionForcingType>::internal_start() {
    const settings::SettingsNode& iso_node = scenario_node["isoraster"];

    // open iso raster
    {
        const std::string& variable = iso_node["variable"].as<std::string>("iso");
        const std::string& filename = iso_node["file"].as<std::string>();
        iso_raster.reset(new RasteredData<int>(filename, variable));
        std::unique_ptr<netCDF::NcFile> file(new netCDF::NcFile(filename, netCDF::NcFile::read));
        const std::string& index_name = iso_node["index"].as<std::string>("index");
        netCDF::NcVar index_var = file->getVar(index_name);
        if (index_var.isNull()) {
            error("Cannot find variable '" << index_name << "' in '" << filename << "'");
        }
        const std::size_t index_size = index_var.getDims()[0].getSize();
        region_forcings.reserve(index_size);
        std::vector<const char*> index_val(index_size);
        index_var.getVar(&index_val[0]);
        for (const auto& r : index_val) {
            const auto region = model()->find_region(r);
            region_forcings.emplace_back(RegionInfo{
                region,                     // region
                0,                          // proxy_sum
                new_region_forcing(region)  // forcing
            });
        }
    }

    const settings::SettingsNode& proxy_node = scenario_node["proxy"];
    // open proxy data
    {
        const std::string& variable = proxy_node["variable"].as<std::string>();
        const std::string& filename = proxy_node["file"].as<std::string>();
        proxy.reset(new RasteredData<FloatType>(filename, variable));
    }

    info("Proxy size is " << (*proxy / *iso_raster) << " of ISO raster");
}

template<class RegionForcingType>
void RasteredScenario<RegionForcingType>::iterate_first_timestep() {
    FloatType total_proxy_sum = 0.0;
    FloatType total_proxy_sum_all = 0.0;
    for (const auto& x : iso_raster->x) {
        for (const auto& y : iso_raster->y) {
            const FloatType proxy_value = proxy->read(x, y);
            if (proxy_value > 0) {
                total_proxy_sum_all += proxy_value;
                const int i = iso_raster->read(x, y);
                if (i >= 0) {
                    region_forcings[i].proxy_sum += proxy_value;
                    total_proxy_sum += proxy_value;
                }
            }
        }
    }
#ifdef DEBUG
    for (auto& r : region_forcings) {
        if (r.region) {
            info(r.region->id() << ": proxy sum: " << r.proxy_sum);
        }
    }
    info("Total proxy sum: " << total_proxy_sum << " (" << total_proxy_sum_all << ")");
#endif
}

template<class RegionForcingType>
void RasteredScenario<RegionForcingType>::read_forcings() {
    total_current_proxy_sum_ = 0.0;
    auto forcing_l = static_cast<RasteredTimeData<FloatType>*>(forcing.get());
    FloatType sub_cnt = *proxy / *forcing_l;
    for (const auto& x : forcing_l->x) {
        for (const auto& y : forcing_l->y) {
            const FloatType proxy_value = proxy->read(x, y);
            if (proxy_value > 0) {
                const int i = iso_raster->read(x, y);
                if (i >= 0) {
                    const FloatType forcing_v = forcing_l->read(x, y);
                    if (!std::isnan(forcing_v)) {
                        RegionInfo& region_info = region_forcings[i];
                        if (region_info.region) {
                            add_cell_forcing(x, y, proxy_value / sub_cnt / sub_cnt, forcing_v, region_info.region, region_info.forcing);
                        }
                    }
                }
            }
        }
    }
}

template<class RegionForcingType>
void RasteredScenario<RegionForcingType>::internal_iterate_start() {
    for (auto& r : region_forcings) {
        if (r.region) {
            reset_forcing(r.region, r.forcing);
        }
    }
}

template<class RegionForcingType>
bool RasteredScenario<RegionForcingType>::internal_iterate_end() {
    for (auto& r : region_forcings) {
        if (r.region && r.proxy_sum > 0) {
            set_region_forcing(r.region, r.forcing, r.proxy_sum);
        }
    }
    return true;
}

template class RasteredScenario<FloatType>;

template class RasteredScenario<std::vector<FloatType>>;
}  // namespace acclimate
