/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
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

#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <iterator>
#include <limits>
#include <ostream>

#include "ModelRun.h"
#include "acclimate.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "netcdfpp.h"
#include "settingsnode.h"
#include "version.h"

namespace acclimate {

NetCDFOutput::NetCDFOutput(Model* model_p, const settings::SettingsNode& settings) : ArrayOutput(model_p, settings, true) {
    flush_freq = settings["flush"].as<TimeStep>(1);
    if (const auto& filename_node = settings["file"]; !filename_node.empty()) {
        filename = filename_node.as<std::string>();
    } else {  // no filename given, use timestamp instead
        std::ostringstream ss;
        ss << std::time(nullptr);
        ss << ".nc";
        filename = ss.str();
    }
    file = std::make_unique<netCDF::File>(filename, 'w');
}

template<std::size_t dim>
void NetCDFOutput::create_group(const char* name,
                                const std::array<netCDF::Dimension, dim + 1>& default_dims,
                                const std::array<const char*, dim>& index_names,
                                const Observable<dim>& observable,
                                std::vector<netCDF::Variable>& nc_variables) {
    if (observable.variables.empty()) {
        return;
    }
    auto group = file->add_group(name);
    std::vector<int> dims(default_dims.size());
    dims[0] = default_dims[0].id();
    if constexpr (dim > 0) {
        for (std::size_t i = 0; i < dim; ++i) {
            if (observable.indices[i].empty()) {
                dims[i + 1] = default_dims[i + 1].id();
            } else {
                dims[i + 1] = group.add_dimension(index_names[i], observable.indices[i].size()).id();
                assert(observable.indices[i].size() == observable.sizes[i]);
                netCDF::Variable var = group.add_variable<std::size_t>(index_names[i], std::vector<int>{dims[i + 1]});
                var.set_compression(false, compression_level);
                var.set<unsigned long long>(observable.indices[i]);
            }
        }
    }
    std::transform(std::begin(observable.variables), std::end(observable.variables), std::back_inserter(nc_variables), [&](const auto& obs_var) {
        auto nc_var = group.add_variable<output_float_t>(obs_var.name, dims);
        nc_var.set_compression(false, compression_level);
        nc_var.set_fill(std::numeric_limits<output_float_t>::quiet_NaN());
        return nc_var;
    });
}

void NetCDFOutput::start() {
    const auto dim_time = file->add_dimension("time");
    const auto dim_sector = file->add_dimension("sector", model()->sectors.size());
    const auto dim_region = file->add_dimension("region", model()->regions.size());
    const auto dim_location = file->add_dimension("location", model()->other_locations.size());
    const auto dim_agent = file->add_dimension("agent", model()->economic_agents.size());
    const auto dim_agent_from = file->add_dimension("agent_from", model()->economic_agents.size());
    const auto dim_agent_to = file->add_dimension("agent_to", model()->economic_agents.size());
    const auto dim_event = file->add_dimension("event");
    const auto dim_event_type = file->add_dimension("event_type", EVENT_NAMES.size());

    auto event_t = file->add_type_compound("event_t", sizeof(ArrayOutput::Event));
    event_t.add_compound_field<decltype(ArrayOutput::Event::time)>("time", offsetof(ArrayOutput::Event, time));
    event_t.add_compound_field<decltype(ArrayOutput::Event::type)>("type", offsetof(ArrayOutput::Event, type));
    event_t.add_compound_field<decltype(ArrayOutput::Event::index1)>("index1", offsetof(ArrayOutput::Event, index1));
    event_t.add_compound_field<decltype(ArrayOutput::Event::index2)>("index2", offsetof(ArrayOutput::Event, index2));
    event_t.add_compound_field<decltype(ArrayOutput::Event::value)>("value", offsetof(ArrayOutput::Event, value));
    var_events = std::make_unique<netCDF::Variable>(file->add_variable("events", event_t, {dim_event}));
    // var_events->set_compression(false, compression_level); //removing compression from events

    var_time = std::make_unique<netCDF::Variable>(file->add_variable<int>("time", {dim_time}));
    var_time->set_compression(false, compression_level);
    var_time->add_attribute("calendar").set<std::string>(model()->run()->calendar());
    var_time->add_attribute("units").set<std::string>(std::string("days since ") + model()->run()->basedate());

    include_events = false;

    {
        auto event_type_var = file->add_variable<std::string>("event_type", {dim_event_type});
        // event_type_var.set_compression(false, compression_level);
        for (std::size_t i = 0; i < EVENT_NAMES.size(); ++i) {
            event_type_var.set<const char*, 1>(EVENT_NAMES[i], {i});
        }
    }

    {
        auto sector_var = file->add_variable<std::string>("sector", {dim_sector});
        // sector_var.set_compression(false, compression_level);
        for (std::size_t i = 0; i < model()->sectors.size(); ++i) {
            sector_var.set<std::string, 1>(model()->sectors[i]->name(), {i});
        }
    }

    {
        auto region_var = file->add_variable<std::string>("region", {dim_region});
        // region_var.set_compression(false, compression_level);
        for (std::size_t i = 0; i < model()->regions.size(); ++i) {
            region_var.set<std::string, 1>(model()->regions[i]->name(), {i});
        }
    }

    {
        const auto dim_location_type = file->add_dimension("location_type", 3);
        file->add_variable<std::string>("location_type", {dim_location_type}).set<std::string>({"region", "sea", "port"});

        struct LocationCompound {
            char name[25];
            std::uint8_t location_type;
        };
        auto location_t = file->add_type_compound<LocationCompound>("location_t");
        location_t.add_compound_field_array<decltype(LocationCompound::name)>("name", offsetof(LocationCompound, name), {25});
        location_t.add_compound_field<decltype(LocationCompound::location_type)>("location_type", offsetof(LocationCompound, location_type));

        auto location_var = file->add_variable("location", location_t, {dim_location});
        // location_var.set_compression(false, compression_level);
        LocationCompound value;
        value.name[sizeof(value.name) / sizeof(value.name[0]) - 1] = '\0';
        for (std::size_t i = 0; i < model()->other_locations.size(); ++i) {
            const auto* location = model()->other_locations[i];
            std::strncpy(value.name, location->name().c_str(), sizeof(value.name) / sizeof(value.name[0]) - 1);
            switch (location->type) {
                case GeoLocation::type_t::REGION:
                    value.location_type = 0;
                    break;
                case GeoLocation::type_t::SEA:
                    value.location_type = 1;
                    break;
                case GeoLocation::type_t::PORT:
                    value.location_type = 2;
                    break;
            }
            location_var.set<LocationCompound, 1>(value, {i});
        }
    }

    {
        const auto dim_agent_type = file->add_dimension("agent_type", 2);
        file->add_variable<std::string>("agent_type", {dim_agent_type}).set<std::string>({"firm", "consumer"});

        struct AgentCompound {
            char name[25];
            std::uint8_t agent_type;
            std::uint32_t sector;
            std::uint32_t region;
        };
        auto agent_t = file->add_type_compound<AgentCompound>("agent_t");
        agent_t.add_compound_field_array<decltype(AgentCompound::name)>("name", offsetof(AgentCompound, name), {25});
        agent_t.add_compound_field<decltype(AgentCompound::agent_type)>("agent_type", offsetof(AgentCompound, agent_type));
        agent_t.add_compound_field<decltype(AgentCompound::sector)>("sector", offsetof(AgentCompound, sector));
        agent_t.add_compound_field<decltype(AgentCompound::region)>("region", offsetof(AgentCompound, region));

        auto agent_var = file->add_variable("agent", agent_t, {dim_agent});
        // agent_var.set_compression(false, compression_level);
        AgentCompound value;
        value.name[sizeof(value.name) / sizeof(value.name[0]) - 1] = '\0';
        for (std::size_t i = 0; i < model()->economic_agents.size(); ++i) {
            const auto* agent = model()->economic_agents[i];
            std::strncpy(value.name, agent->name().c_str(), sizeof(value.name) / sizeof(value.name[0]) - 1);
            if (agent->is_firm()) {
                value.agent_type = 0;
                value.sector = agent->as_firm()->sector->id.index();
            } else {
                value.agent_type = 1;
                value.sector = 0;
            }
            value.region = agent->region->id.index();
            agent_var.set<AgentCompound, 1>(value, {i});
        }
    }

    file->add_attribute("settings").set<std::string>(model()->run()->settings_string());
    file->add_attribute("start_time").set<std::string>(model()->run()->now());
    file->add_attribute("max_threads").set<int>(model()->run()->thread_count());
    file->add_attribute("version").set<std::string>(version);
    if (has_diff) {
        file->add_attribute("diff").set<std::string>(git_diff);
    }

    auto options_group = file->add_group("options");
    for (const auto& option : options::options) {
        options_group.add_attribute(option.name).set<unsigned char>(option.value ? 1 : 0);
    }

    create_group<0>("model", {dim_time}, {}, obs_model, vars_model);
    create_group<1>("firms", {dim_time, dim_agent}, {"firm_index"}, obs_firms, vars_firms);
    create_group<1>("consumers", {dim_time, dim_agent}, {"consumer_index"}, obs_consumers, vars_consumers);
    create_group<1>("sectors", {dim_time, dim_sector}, {"sector_index"}, obs_sectors, vars_sectors);
    create_group<1>("regions", {dim_time, dim_region}, {"region_index"}, obs_regions, vars_regions);
    create_group<1>("locations", {dim_time, dim_location}, {"location_index"}, obs_locations, vars_locations);
    create_group<2>("storages", {dim_time, dim_sector, dim_agent}, {"sector_input_index", "agent_index"}, obs_storages, vars_storages);
    create_group<2>("flows", {dim_time, dim_agent_from, dim_agent_to}, {"agent_from_index", "agent_to_index"}, obs_flows, vars_flows);
}

void NetCDFOutput::end() {
    file->add_attribute("end_time").set<std::string>(model()->run()->now());
    file->close();
}

template<>
void NetCDFOutput::write_variables(const Observable<0>& observable, std::vector<netCDF::Variable>& nc_variables) {
    for (std::size_t i = 0; i < nc_variables.size(); ++i) {
        nc_variables[i].set<output_float_t, 1>(observable.variables[i].data[0], {model()->timestep()});
    }
}

template<>
void NetCDFOutput::write_variables(const Observable<1>& observable, std::vector<netCDF::Variable>& nc_variables) {
    for (std::size_t i = 0; i < nc_variables.size(); ++i) {
        nc_variables[i].set<output_float_t, 2>(observable.variables[i].data, {model()->timestep(), 0}, {1, observable.sizes[0]});
    }
}

template<>
void NetCDFOutput::write_variables(const Observable<2>& observable, std::vector<netCDF::Variable>& nc_variables) {
    for (std::size_t i = 0; i < nc_variables.size(); ++i) {
        nc_variables[i].set<output_float_t, 3>(observable.variables[i].data, {model()->timestep(), 0, 0}, {1, observable.sizes[0], observable.sizes[1]});
    }
}

void NetCDFOutput::iterate() {
    var_time->set<output_float_t, 1>(to_float(model()->time()), {model()->timestep()});

    ArrayOutput::iterate();

    write_variables(obs_model, vars_model);
    write_variables(obs_firms, vars_firms);
    write_variables(obs_consumers, vars_consumers);
    write_variables(obs_sectors, vars_sectors);
    write_variables(obs_regions, vars_regions);
    write_variables(obs_locations, vars_locations);
    write_variables(obs_storages, vars_storages);
    write_variables(obs_flows, vars_flows);

    if (!events.empty()) {
        var_events->set<Event, 1>(events, {event_cnt}, {events.size()});
        event_cnt += events.size();
    }

    if (flush_freq > 0) {
        if ((model()->timestep() % flush_freq) == 0) {
            file->sync();
        }
    }
}

void NetCDFOutput::checkpoint_stop() { file->close(); }

void NetCDFOutput::checkpoint_resume() { file->open(filename, 'a'); }

}  // namespace acclimate
