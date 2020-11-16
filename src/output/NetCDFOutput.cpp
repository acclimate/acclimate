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

#include <array>
#include <ctime>
#include <limits>
#include <ostream>
#include <utility>

#include "ModelRun.h"
#include "acclimate.h"
#include "model/Model.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "settingsnode.h"
#include "version.h"

struct tm;

namespace acclimate {

NetCDFOutput::NetCDFOutput(const settings::SettingsNode& settings_p, Model* model_p, settings::SettingsNode output_node_p)
    : ArrayOutput(settings_p, model_p, std::move(output_node_p), false) {
    flush_freq = 1;
    event_cnt = 0;
    const settings::SettingsNode& run = settings_p["run"];
    calendar = run["calendar"].as<std::string>("standard");
    if (run.has("baseyear")) {
        time_units = std::string("days since ") + std::to_string(run["baseyear"].template as<unsigned int>()) + "-1-1";
    } else {
        time_units = "days since 0-1-1";
    }
}

void NetCDFOutput::initialize() {
    ArrayOutput::initialize();
    flush_freq = output_node["flush"].template as<TimeStep>();
    if (!output_node.has("file")) {
        std::ostringstream ss;
        ss << std::time(nullptr);
        ss << ".nc";
        filename = ss.str();
    } else {
        filename = output_node["file"].template as<std::string>();
    }
    file.open(filename, 'w');

    dim_time = file.add_dimension("time").id();
    dim_sector = file.add_dimension("sector", sectors_size).id();
    dim_region = file.add_dimension("region", regions_size).id();
    dim_firm_names = file.add_dimension("firm_names", firms_size).id();
    const auto dim_event = file.add_dimension("event");
    const auto dim_event_type = file.add_dimension("event_type", EVENT_NAMES.size());
    auto event_compound_type = file.add_type_compound("event_compound_type", sizeof(typename ArrayOutput::Event));
    event_compound_type.add_compound_field<decltype(ArrayOutput::Event::time)>("time", offsetof(typename ArrayOutput::Event, time));
    event_compound_type.add_compound_field<decltype(ArrayOutput::Event::type)>("type", offsetof(typename ArrayOutput::Event, type));
    event_compound_type.add_compound_field<decltype(ArrayOutput::Event::sector_from)>("sector_from", offsetof(typename ArrayOutput::Event, sector_from));
    event_compound_type.add_compound_field<decltype(ArrayOutput::Event::region_from)>("region_from", offsetof(typename ArrayOutput::Event, region_from));
    event_compound_type.add_compound_field<decltype(ArrayOutput::Event::sector_to)>("sector_to", offsetof(typename ArrayOutput::Event, sector_to));
    event_compound_type.add_compound_field<decltype(ArrayOutput::Event::region_to)>("region_to", offsetof(typename ArrayOutput::Event, region_to));
    event_compound_type.add_compound_field<decltype(ArrayOutput::Event::value)>("value", offsetof(typename ArrayOutput::Event, value));
    var_events = std::make_unique<netCDF::Variable>(file.add_variable("events", event_compound_type.id(), {dim_event}));
    var_events->set_compression(false, compression_level);

    var_time_variable = std::make_unique<netCDF::Variable>(file.add_variable<int>("time", std::vector<int>{dim_time}));
    var_time_variable->set_compression(false, compression_level);
    var_time_variable->add_attribute("calendar").set<std::string>(calendar);
    var_time_variable->add_attribute("units").set<std::string>(time_units);

    include_events = true;
    auto event_type_var = file.add_variable<std::string>("event_types", {dim_event_type});
    event_type_var.set_compression(false, compression_level);
    for (std::size_t i = 0; i < EVENT_NAMES.size(); ++i) {
        event_type_var.set<const char*, 1>(EVENT_NAMES[i], {i});
    }

    auto sector_var = file.add_variable<std::string>("sector", std::vector<int>{dim_sector});
    sector_var.set_compression(false, compression_level);
    for (std::size_t i = 0; i < model()->sectors.size(); ++i) {
        sector_var.set<std::string, 1>(model()->sectors[i]->id(), {i});
    }

    auto region_var = file.add_variable<std::string>("region", std::vector<int>{dim_region});
    region_var.set_compression(false, compression_level);
    for (std::size_t i = 0; i < model()->regions.size(); ++i) {
        region_var.set<std::string, 1>(model()->regions[i]->id(), {i});
    }

    auto firm_name_var = file.add_variable<std::string>("firmname", std::vector<int>{dim_firm_names});
    firm_name_var.set_compression(false, compression_level);
    for (std::size_t i = 0; i < model()->economic_agents.size(); ++i) {
        firm_name_var.set<std::string, 1>(model()->economic_agents[i].first->id(), {i});
    }
}

void NetCDFOutput::internal_write_header(tm* timestamp, unsigned int max_threads) {
    std::string str = std::asctime(timestamp);
    str.erase(str.end() - 1);
    file.add_attribute("start_time").set<std::string>(str);
    file.add_attribute("max_threads").set<int>(max_threads);
    file.add_attribute("version").set<std::string>(version);
    auto options_group = file.add_group("options");
    for (const auto& option : options::options) {
        options_group.add_attribute(option.name).set<unsigned char>(option.value ? 1 : 0);
    }
    if (has_diff) {
        file.add_attribute("diff").set<std::string>(git_diff);
    }
}

void NetCDFOutput::internal_write_footer(tm* duration) { file.add_attribute("duration").set<int>(std::mktime(duration)); }

void NetCDFOutput::internal_write_settings() { file.add_attribute("settings").set<std::string>(settings_string); }

void NetCDFOutput::create_variable_meta(typename ArrayOutput::Variable& v, const hstring& path, const hstring& name, const hstring& suffix) {
    std::vector<std::size_t> index;
    std::vector<std::size_t> sizes;
    std::vector<int> dims;
    index.push_back(0);
    sizes.push_back(1);
    dims.push_back(dim_time);
    for (const auto& t : stack) {
        if (t.sector != nullptr) {
            index.push_back(0);
            sizes.push_back(sectors_size);
            dims.push_back(dim_sector);
        }
        if (t.region != nullptr) {
            index.push_back(0);
            sizes.push_back(regions_size);
            dims.push_back(dim_region);
        }
        if (t.firmname != nullptr) {
            index.push_back(0);
            sizes.push_back(firms_size);
            dims.push_back(dim_firm_names);
        }
    }
    auto& group = create_group(path);
    auto nc_var = group.add_variable<double>(std::string(name) + std::string(suffix), dims);
    nc_var = nc_var;
    nc_var.set_fill(std::numeric_limits<FloatType>::quiet_NaN());
    nc_var.set_compression(false, compression_level);
    v.meta = new VariableMeta{std::move(index), std::move(sizes), std::move(nc_var)};
}

netCDF::Group& NetCDFOutput::create_group(const hstring& name) {
    auto group_it = groups.find(name);
    if (group_it == groups.end()) {
        group_it = groups.emplace(name, file.add_group(name)).first;
    }
    return (*group_it).second;
}

void NetCDFOutput::internal_iterate_begin() {
    ArrayOutput::internal_iterate_begin();
    var_time_variable->set<FloatType, 1>(to_float(model()->time()), {model()->timestep()});
}

void NetCDFOutput::internal_iterate_end() {
    for (auto& var : variables) {
        auto* meta = static_cast<VariableMeta*>(var.second.meta);
        meta->index[0] = model()->timestep();
        meta->sizes[0] = 1;
        meta->nc_var.set<FloatType>(var.second.data, &meta->index[0], &meta->sizes[0]);
    }
    if (flush_freq > 0) {
        if ((model()->timestep() % flush_freq) == 0) {
            flush();
        }
    }
}

void NetCDFOutput::internal_end() { file.close(); }

bool NetCDFOutput::internal_handle_event(typename ArrayOutput::Event& event) {
    netcdf_event_lock.call([&]() {
        var_events->set<Event, 1>(event, {event_cnt});
        ++event_cnt;
    });
    return false;
}

void NetCDFOutput::checkpoint_stop() { file.close(); }

void NetCDFOutput::checkpoint_resume() { file.open(filename, 'a'); }

void NetCDFOutput::flush() {
    if (file.is_open()) {
        file.sync();
    }
}

NetCDFOutput::~NetCDFOutput() {
    for (auto& var : variables) {
        delete static_cast<VariableMeta*>(var.second.meta);
    }
}

}  // namespace acclimate
