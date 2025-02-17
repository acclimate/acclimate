// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "output/NetCDFOutput.h"

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

NetCDFOutput::NetCDFOutput(Model* model, const settings::SettingsNode& settings) : ArrayOutput(model, settings, true) {
    flush_freq_ = settings["flush"].as<TimeStep>(1);
    if (const auto& filename_node = settings["file_"]; !filename_node.empty()) {
        filename_ = filename_node.as<std::string>();
    } else {  // no filename_ given, use timestamp instead
        std::ostringstream ss;
        ss << std::time(nullptr);
        ss << ".nc";
        filename_ = ss.str();
    }
    file_ = std::make_unique<netCDF::File>(filename_, 'w');
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
    auto group = file_->add_group(name);
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
                var.set_compression(false, compression_level_);
                var.set<unsigned long long>(observable.indices[i]);
            }
        }
    }
    std::transform(std::begin(observable.variables), std::end(observable.variables), std::back_inserter(nc_variables), [&](const auto& obs_var) {
        auto nc_var = group.add_variable<output_float_t>(obs_var.name, dims);
        nc_var.set_compression(false, compression_level_);
        nc_var.set_fill(std::numeric_limits<output_float_t>::quiet_NaN());
        return nc_var;
    });
}

void NetCDFOutput::start() {
    const auto dim_time = file_->add_dimension("time");
    const auto dim_sector = file_->add_dimension("sector", model()->sectors.size());
    const auto dim_region = file_->add_dimension("region", model()->regions.size());
    const auto dim_location = file_->add_dimension("location", model()->other_locations.size());
    const auto dim_agent = file_->add_dimension("agent", model()->economic_agents.size());
    const auto dim_agent_from = file_->add_dimension("agent_from", model()->economic_agents.size());
    const auto dim_agent_to = file_->add_dimension("agent_to", model()->economic_agents.size());
    const auto dim_event = file_->add_dimension("event");
    const auto dim_event_type = file_->add_dimension("event_type", EVENT_NAMES.size());

    auto event_t = file_->add_type_compound("event_t", sizeof(ArrayOutput::Event));
    event_t.add_compound_field<decltype(ArrayOutput::Event::time)>("time", offsetof(ArrayOutput::Event, time));
    event_t.add_compound_field<decltype(ArrayOutput::Event::type)>("type", offsetof(ArrayOutput::Event, type));
    event_t.add_compound_field<decltype(ArrayOutput::Event::index1)>("index1", offsetof(ArrayOutput::Event, index1));
    event_t.add_compound_field<decltype(ArrayOutput::Event::index2)>("index2", offsetof(ArrayOutput::Event, index2));
    event_t.add_compound_field<decltype(ArrayOutput::Event::value)>("value", offsetof(ArrayOutput::Event, value));
    var_events_ = std::make_unique<netCDF::Variable>(file_->add_variable("events_", event_t, {dim_event}));

    var_time_ = std::make_unique<netCDF::Variable>(file_->add_variable<int>("time", {dim_time}));
    var_time_->set_compression(false, compression_level_);
    var_time_->add_attribute("calendar").set<std::string>(model()->run()->calendar());
    var_time_->add_attribute("units").set<std::string>(std::string("days since ") + model()->run()->basedate());

    include_events_ = false;

    {
        auto event_type_var = file_->add_variable<std::string>("event_type", {dim_event_type});
        // event_type_var.set_compression(false, compression_level_);
        for (std::size_t i = 0; i < EVENT_NAMES.size(); ++i) {
            event_type_var.set<const char*, 1>(EVENT_NAMES[i], {i});
        }
    }

    {
        auto sector_var = file_->add_variable<std::string>("sector", {dim_sector});
        // sector_var.set_compression(false, compression_level_);
        for (std::size_t i = 0; i < model()->sectors.size(); ++i) {
            sector_var.set<std::string, 1>(model()->sectors[i]->name(), {i});
        }
    }

    {
        auto region_var = file_->add_variable<std::string>("region", {dim_region});
        // region_var.set_compression(false, compression_level_);
        for (std::size_t i = 0; i < model()->regions.size(); ++i) {
            region_var.set<std::string, 1>(model()->regions[i]->name(), {i});
        }
    }

    {
        const auto dim_location_type = file_->add_dimension("location_type", 3);
        file_->add_variable<std::string>("location_type", {dim_location_type}).set<std::string>({"region", "sea", "port"});

        struct LocationCompound {
            char name[25];
            std::uint8_t location_type;
        };
        auto location_t = file_->add_type_compound<LocationCompound>("location_t");
        location_t.add_compound_field_array<decltype(LocationCompound::name)>("name", offsetof(LocationCompound, name), {25});
        location_t.add_compound_field<decltype(LocationCompound::location_type)>("location_type", offsetof(LocationCompound, location_type));

        auto location_var = file_->add_variable("location", location_t, {dim_location});
        // location_var.set_compression(false, compression_level_);
        LocationCompound value{};
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
        const auto dim_agent_type = file_->add_dimension("agent_type", 2);
        file_->add_variable<std::string>("agent_type", {dim_agent_type}).set<std::string>({"firm", "consumer"});

        struct AgentCompound {
            char name[25];
            std::uint8_t agent_type;
            std::uint32_t sector;
            std::uint32_t region;
        };
        auto agent_t = file_->add_type_compound<AgentCompound>("agent_t");
        agent_t.add_compound_field_array<decltype(AgentCompound::name)>("name", offsetof(AgentCompound, name), {25});
        agent_t.add_compound_field<decltype(AgentCompound::agent_type)>("agent_type", offsetof(AgentCompound, agent_type));
        agent_t.add_compound_field<decltype(AgentCompound::sector)>("sector", offsetof(AgentCompound, sector));
        agent_t.add_compound_field<decltype(AgentCompound::region)>("region", offsetof(AgentCompound, region));

        auto agent_var = file_->add_variable("agent", agent_t, {dim_agent});
        // agent_var.set_compression(false, compression_level_);
        AgentCompound value{};
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

    file_->add_attribute("settings").set<std::string>(model()->run()->settings_string());
    file_->add_attribute("start_time").set<std::string>(model()->run()->now());
    file_->add_attribute("max_threads").set<int>(model()->run()->thread_count());
    file_->add_attribute("version").set<std::string>(Version::version);
    if (Version::has_diff) {
        file_->add_attribute("diff").set<std::string>(Version::git_diff);
    }

    auto options_group = file_->add_group("options");
    for (const auto& option : Options::options) {
        options_group.add_attribute(option.name).set<unsigned char>(option.value ? 1 : 0);
    }

    create_group<0>("model", {dim_time}, {}, obs_model_, vars_model_);
    create_group<1>("firms", {dim_time, dim_agent}, {"firm_index"}, obs_firms_, vars_firms_);
    create_group<1>("consumers", {dim_time, dim_agent}, {"consumer_index"}, obs_consumers_, vars_consumers_);
    create_group<1>("sectors", {dim_time, dim_sector}, {"sector_index"}, obs_sectors_, vars_sectors_);
    create_group<1>("regions", {dim_time, dim_region}, {"region_index"}, obs_regions_, vars_regions_);
    create_group<1>("locations", {dim_time, dim_location}, {"location_index"}, obs_locations_, vars_locations_);
    create_group<2>("storages", {dim_time, dim_sector, dim_agent}, {"sector_input_index", "agent_index"}, obs_storages_, vars_storages_);
    create_group<2>("flows", {dim_time, dim_agent_from, dim_agent_to}, {"agent_from_index", "agent_to_index"}, obs_flows_, vars_flows_);
}

void NetCDFOutput::end() {
    file_->add_attribute("end_time").set<std::string>(model()->run()->now());
    file_->close();
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
    var_time_->set<output_float_t, 1>(to_float(model()->time()), {model()->timestep()});

    ArrayOutput::iterate();

    write_variables(obs_model_, vars_model_);
    write_variables(obs_firms_, vars_firms_);
    write_variables(obs_consumers_, vars_consumers_);
    write_variables(obs_sectors_, vars_sectors_);
    write_variables(obs_regions_, vars_regions_);
    write_variables(obs_locations_, vars_locations_);
    write_variables(obs_storages_, vars_storages_);
    write_variables(obs_flows_, vars_flows_);

    if (!events_.empty()) {
        var_events_->set<Event, 1>(events_, {event_cnt_}, {events_.size()});
        event_cnt_ += events_.size();
    }

    if (flush_freq_ > 0) {
        if ((model()->timestep() % flush_freq_) == 0) {
            file_->sync();
        }
    }
}

void NetCDFOutput::checkpoint_stop() { file_->close(); }

void NetCDFOutput::checkpoint_resume() { file_->open(filename_, 'a'); }

}  // namespace acclimate
