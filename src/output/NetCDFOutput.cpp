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

#include "output/NetCDFOutput.h"
#include "model/Model.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "scenario/Scenario.h"
#include "variants/ModelVariants.h"
#include "version.h"

#ifdef ACCLIMATE_HAS_DIFF
extern const char* acclimate_git_diff;
#endif

namespace acclimate {

template<class ModelVariant>
NetCDFOutput<ModelVariant>::NetCDFOutput(const settings::SettingsNode& settings_p,
                                         Model<ModelVariant>* model_p,
                                         Scenario<ModelVariant>* scenario_p,
                                         const settings::SettingsNode& output_node_p)
    : ArrayOutput<ModelVariant>(settings_p, model_p, scenario_p, output_node_p, false) {
    flush = 1;
    event_cnt = 0;
}

template<class ModelVariant>
void NetCDFOutput<ModelVariant>::initialize() {
    ArrayOutput<ModelVariant>::initialize();
    flush = output_node["flush"].template as<TimeStep>();
    std::string filename;
    if (!output_node.has("file")) {
        std::ostringstream ss;
        ss << time(nullptr);
        ss << ".nc";
        filename = ss.str();
    } else {
        filename = output_node["file"].template as<std::string>();
    }
    file.reset(new netCDF::NcFile(filename, netCDF::NcFile::replace, netCDF::NcFile::nc4));
    if (!file) {
        error("Could not create output file " << filename);
    }

    dim_time = file->addDim("time");
    dim_sector = file->addDim("sector", sectors_size);
    dim_region = file->addDim("region", regions_size);
    netCDF::NcDim dim_event = file->addDim("event");
    netCDF::NcDim dim_event_type = file->addDim("event_type", Acclimate::event_names.size());
    netCDF::NcCompoundType event_compound_type = file->addCompoundType("event_compound_type", sizeof(typename ArrayOutput<ModelVariant>::Event));
    event_compound_type.addMember("time", netCDF::NcType::nc_UINT, offsetof(typename ArrayOutput<ModelVariant>::Event, time));
    event_compound_type.addMember("type", netCDF::NcType::nc_UBYTE, offsetof(typename ArrayOutput<ModelVariant>::Event, type));
    event_compound_type.addMember("sector_from", netCDF::NcType::nc_INT, offsetof(typename ArrayOutput<ModelVariant>::Event, sector_from));
    event_compound_type.addMember("region_from", netCDF::NcType::nc_INT, offsetof(typename ArrayOutput<ModelVariant>::Event, region_from));
    event_compound_type.addMember("sector_to", netCDF::NcType::nc_INT, offsetof(typename ArrayOutput<ModelVariant>::Event, sector_to));
    event_compound_type.addMember("region_to", netCDF::NcType::nc_INT, offsetof(typename ArrayOutput<ModelVariant>::Event, region_to));
    event_compound_type.addMember("value", netCDF::NcType::nc_DOUBLE, offsetof(typename ArrayOutput<ModelVariant>::Event, value));
    var_events = file->addVar("events", event_compound_type, {dim_event});
    var_events.setCompression(false, true, 7);

    var_time_variable = file->addVar("time", netCDF::NcType::nc_INT, {dim_time});
    var_time_variable.setCompression(false, true, 7);

    include_events = true;
    const auto& event_type_var = file->addVar("event_types", netCDF::NcType::nc_STRING, {dim_event_type});
    event_type_var.setCompression(false, true, 7);
    for (std::size_t i = 0; i < Acclimate::event_names.size(); ++i) {
        event_type_var.putVar({i}, std::string(Acclimate::event_names[i]));
    }

    const auto& sector_var = file->addVar("sector", netCDF::NcType::nc_STRING, {dim_sector});
    sector_var.setCompression(false, true, 7);
    for (std::size_t i = 0; i < model->sectors_C.size(); ++i) {
        sector_var.putVar({i}, model->sectors_C[i]->id());
    }

    const auto& region_var = file->addVar("region", netCDF::NcType::nc_STRING, {dim_region});
    region_var.setCompression(false, true, 7);
    for (std::size_t i = 0; i < model->regions_R.size(); ++i) {
        region_var.putVar({i}, model->regions_R[i]->id());
    }
}

template<class ModelVariant>
void NetCDFOutput<ModelVariant>::internal_start() {
    var_time_variable.putAtt("calendar", scenario->calendar_str());
    var_time_variable.putAtt("units", scenario->time_units_str());
}

template<class ModelVariant>
void NetCDFOutput<ModelVariant>::internal_write_header(tm* timestamp, int max_threads) {
    std::string str = asctime(timestamp);
    str.erase(str.end() - 1);
    file->putAtt("start_time", str);
    file->putAtt("max_threads", netCDF::NcType::nc_INT, max_threads);
    file->putAtt("version", ACCLIMATE_VERSION);
    file->putAtt("options", ACCLIMATE_OPTIONS);
#ifdef ACCLIMATE_HAS_DIFF
    file->putAtt("diff", acclimate_git_diff);
#endif
}

template<class ModelVariant>
void NetCDFOutput<ModelVariant>::internal_write_footer(tm* duration) {
    file->putAtt("duration", netCDF::NcType::nc_INT, mktime(duration));
}

template<class ModelVariant>
void NetCDFOutput<ModelVariant>::internal_write_settings() {
    std::ostringstream ss;
    ss << settings;
    file->putAtt("settings", ss.str());
}

template<class ModelVariant>
void NetCDFOutput<ModelVariant>::create_variable_meta(typename ArrayOutput<ModelVariant>::Variable& v, const std::string& path, const std::string& name) {
    auto meta = new VariableMeta();
    std::vector<netCDF::NcDim> dims;
    meta->index.push_back(0);
    meta->sizes.push_back(1);
    dims.push_back(dim_time);
    for (const auto& t : stack) {
        if (t.sector) {
            meta->index.push_back(0);
            meta->sizes.push_back(sectors_size);
            dims.push_back(dim_sector);
        }
        if (t.region) {
            meta->index.push_back(0);
            meta->sizes.push_back(regions_size);
            dims.push_back(dim_region);
        }
    }
    netCDF::NcGroup& group = create_group(path);
    netCDF::NcVar nc_var = group.addVar(name, netCDF::NcType::nc_DOUBLE, dims);
    meta->nc_var = nc_var;
    meta->nc_var.setFill(true, std::numeric_limits<FloatType>::quiet_NaN());
    meta->nc_var.setCompression(false, true, 7);
    v.meta = meta;
}

template<class ModelVariant>
netCDF::NcGroup& NetCDFOutput<ModelVariant>::create_group(const std::string& name) {
    auto group_it = groups.find(name);
    if (group_it == groups.end()) {
        group_it = groups.emplace(name, file->addGroup(name)).first;
    }
    return (*group_it).second;
}

template<class ModelVariant>
void NetCDFOutput<ModelVariant>::internal_iterate_begin() {
    ArrayOutput<ModelVariant>::internal_iterate_begin();
    var_time_variable.putVar({model->timestep()}, to_float(model->time()));
}

template<class ModelVariant>
void NetCDFOutput<ModelVariant>::internal_iterate_end() {
    for (auto& var : variables) {
        auto* meta = static_cast<VariableMeta*>(var.second.meta);
        meta->index[0] = model->timestep();
        meta->sizes[0] = 1;
        meta->nc_var.putVar(meta->index, meta->sizes, &var.second.data[0]);
    }
    if (flush > 0) {
        if ((model->timestep() % flush) == 0) {
            file->sync();
        }
    }
}

template<class ModelVariant>
void NetCDFOutput<ModelVariant>::internal_end() {
    file->close();
    file.reset();
}

template<class ModelVariant>
bool NetCDFOutput<ModelVariant>::internal_handle_event(typename ArrayOutput<ModelVariant>::Event& event) {
    netcdf_event_lock.call([&]() {
                               var_events.putVar({event_cnt}, &event);
                               ++event_cnt;
                           });
    return false;
}

template<class ModelVariant>
NetCDFOutput<ModelVariant>::~NetCDFOutput() {
    for (auto& var : variables) {
        delete static_cast<VariableMeta*>(var.second.meta);
    }
}

INSTANTIATE_BASIC(NetCDFOutput);
}  // namespace acclimate
