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

template<class ModelVariant>
RasteredScenario<ModelVariant>::RasteredScenario(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p)
    : ExternalScenario<ModelVariant>(settings_p, model_p) {}

template<class ModelVariant>
ExternalForcing* RasteredScenario<ModelVariant>::read_forcing_file(const std::string& filename, const std::string& variable_name) {
    auto result = new RasteredTimeData<FloatType>(filename, variable_name);
    if (!result->is_compatible(*iso_raster)) {
        info("ISO raster size is " << (*iso_raster / *result) << " of forcing");
        error("Forcing and ISO raster not compatible in raster resolution");
    }
    info("Population size is " << (*population / *result) << " of forcing");
    return result;
}

template<class ModelVariant>
void RasteredScenario<ModelVariant>::internal_start() {
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
            RegionInfo tmp = {
                model->find_region(r),  // region
                0,                      // population
                0,                      // people_affected
                0                       // forcing
            };
            region_forcings.emplace_back(tmp);
        }
    }

    // open population data
    {
        const std::string& variable = scenario_node["population"]["variable"].as<std::string>("population");
        const std::string& filename = scenario_node["population"]["file"].as<std::string>();
        population.reset(new RasteredData<FloatType>(filename, variable));
    }

    info("Population size is " << (*population / *iso_raster) << " of ISO raster");
}

template<class ModelVariant>
void RasteredScenario<ModelVariant>::iterate_first_timestep() {
    FloatType total_population = 0.0;
    FloatType total_population_all = 0.0;
    for (const auto& x : iso_raster->x) {
        for (const auto& y : iso_raster->y) {
            int i = iso_raster->read(x, y);
            FloatType population_v = population->read(x, y);
            if (population_v > 0) {
                total_population_all += population_v;
                if (i >= 0) {
                    region_forcings[i].population += population_v;
                    if (region_forcings[i].region) {
                        total_population += population_v;
                    }
                }
            }
        }
    }
#ifdef DEBUG
    for (auto& r : region_forcings) {
        if (r.region) {
            info(std::string(*r.region) << ": population: " << r.population);
        }
    }
    info("Total population: " << total_population << " (" << total_population_all << ")");
#endif
}

template<class ModelVariant>
void RasteredScenario<ModelVariant>::read_forcings() {
    auto forcing_l = static_cast<RasteredTimeData<FloatType>*>(forcing.get());
    FloatType sub_cnt = *population / *forcing_l;
    for (const auto& x : forcing_l->x) {
        for (const auto& y : forcing_l->y) {
            FloatType forcing_v = forcing_l->read(x, y);
            if (forcing_v > 0) {
                int i = iso_raster->read(x, y);
                if (i >= 0) {
                    FloatType population_v = population->read(x, y);
                    if (population_v > 0) {
                        FloatType affected_people = get_affected_population_per_cell(x, y, population_v / sub_cnt / sub_cnt, forcing_v);
                        region_forcings[i].forcing += affected_people;
                    }
                }
            }
        }
    }
}

template<class ModelVariant>
bool RasteredScenario<ModelVariant>::internal_iterate() {
    people_affected_ = 0.0;
    for (auto& r : region_forcings) {
        if (r.population > 0 && r.region) {
            set_forcing(r.region, r.forcing / r.population);
            r.people_affected = r.forcing;
            people_affected_ += r.people_affected;
            r.forcing = 0.0;
        }
    }
    info("People affected: " << people_affected_);
    return true;
}

INSTANTIATE_BASIC(RasteredScenario);
}  // namespace acclimate
