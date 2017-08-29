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

#include "input/ModelInitializer.h"
#include <fstream>
#include "MRIOTable.h"
#include "model/BusinessConnection.h"
#include "model/CapacityManagerPrices.h"
#include "model/Consumer.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/GeographicPoint.h"
#include "model/Model.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "netcdf_headers.h"
#include "optimization.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
ModelInitializer<ModelVariant>::ModelInitializer(Model<ModelVariant>* model_p, const settings::SettingsNode& settings_p)
    : model(model_p), settings(settings_p), transport_time(0) {
    const settings::SettingsNode& parameters = settings["parameters"];
    model->delta_t(parameters["delta_t"].as<Time>());
    model->no_self_supply(parameters["no_self_supply"].as<bool>());
}

template<class ModelVariant>
settings::SettingsNode ModelInitializer<ModelVariant>::get_named_property(const std::string& tag_name,
                                                                          const std::string& node_name,
                                                                          const std::string& property_name) const {
    for (const auto& parameters : settings[tag_name].as_sequence()) {
        if (parameters.has("name")) {
            if (parameters["name"].template as<std::string>() == node_name && parameters.has(property_name)) {
                return parameters[property_name];
            }
        } else {
            return parameters[property_name];
        }
    }
    error("Error in settings: " << property_name << " not found for name=='" << node_name << "' in \n" << settings[tag_name]);
}

template<class ModelVariant>
settings::SettingsNode ModelInitializer<ModelVariant>::get_firm_property(const std::string& sector_name,
                                                                         const std::string& region_name,
                                                                         const std::string& property_name) const {
    settings::SettingsNode result;
    unsigned char found_prio = 0;
    for (const auto& parameters : settings["firm"].as_sequence()) {
        if (parameters.has("sector")) {
            if (parameters.has("region")) {
                if (parameters["sector"].template as<std::string>() == sector_name && parameters["region"].template as<std::string>() == region_name
                    && parameters.has(property_name)) {
                    return parameters[property_name];
                }
            } else {
                if (parameters["sector"].template as<std::string>() == sector_name) {
                    result = parameters[property_name];
                    found_prio = 2;
                }
            }
        } else if (parameters.has("region")) {
            if (found_prio <= 1 && parameters["region"].template as<std::string>() == region_name) {
                result = parameters[property_name];
                found_prio = 1;
            }
        } else if (found_prio == 0) {
            result = parameters[property_name];
        }
    }
    if (result.empty()) {
        error("Error in settings: " << property_name << " not found for sector=='" << sector_name << "', region=='" << region_name << "' in \n"
                                    << settings["firm"]);
    }
    return result;
}

template<class ModelVariant>
Firm<ModelVariant>* ModelInitializer<ModelVariant>::add_firm(Sector<ModelVariant>* sector, Region<ModelVariant>* region) {
    auto firm =
        new Firm<ModelVariant>(sector, region, static_cast<Ratio>(get_firm_property(*sector, *region, "possible_overcapacity_ratio").template as<double>()));
    region->economic_agents.emplace_back(firm);
    sector->firms_N.push_back(firm);
    return firm;
}

template<class ModelVariant>
Consumer<ModelVariant>* ModelInitializer<ModelVariant>::add_consumer(Region<ModelVariant>* region) {
    auto consumer = new Consumer<ModelVariant>(region);
    region->economic_agents.emplace_back(consumer);
    return consumer;
}

template<class ModelVariant>
Region<ModelVariant>* ModelInitializer<ModelVariant>::add_region(const std::string& name) {
    Region<ModelVariant>* region = model->find_region(name);
    if (!region) {
        region = new Region<ModelVariant>(model, name, model->regions_R.size());
        model->regions_R.emplace_back(region);
        switch (transport_time_base) {
            case TransportTimeBase::CONST:
            case TransportTimeBase::CENTROIDS:
                for (auto& region_to : model->regions_R) {
                    TransportDelay transport_delay_tau;
                    if (transport_time_base == TransportTimeBase::CENTROIDS) {
                        try {
                            const GeographicPoint& c = region_centroids.at(name);
                            region->set_centroid(new GeographicPoint("centroid", c.lon(), c.lat()));
                        } catch (std::out_of_range&) {
                            error("Centroid for " << name << " not found");
                        }
                        unsigned int transport_delay_tau_int;
                        const FloatType distance = region->centroid()->distance_to(*region_to->centroid());
                        if (distance >= threshold_road_transport) {
                            transport_delay_tau_int = iround(distance / avg_water_speed / 24. / to_float(model->delta_t()));
                        } else {
                            transport_delay_tau_int = iround(distance / avg_road_speed / 24. / to_float(model->delta_t()));
                        }
                        if (transport_delay_tau_int > 254) {
                            error("Transport delay too large: " << transport_delay_tau_int);
                        }
                        transport_delay_tau = std::max(static_cast<unsigned int>(1), transport_delay_tau_int);
                    } else {
                        transport_delay_tau = transport_time;
                    }
                    auto inf = new Infrastructure<ModelVariant>(transport_delay_tau);
                    region->connections.push_back(inf);
                    region_to->connections.push_back(inf);
                    inf->connections.push_back(region);
                    inf->connections.push_back(region_to.get());
                    model->infrastructure_G.emplace_back(inf);
                    Path<ModelVariant> path = {
                        transport_delay_tau,  // distance
                        inf                   // infrastructure
                    };
                    region->paths.emplace(region_to.get(), path);
                    region_to->paths.emplace(region, path);
                }
                break;
            case TransportTimeBase::PRECALCULATED:
                break;
        }
    }
    return region;
}

template<>
Sector<VariantBasic>* ModelInitializer<VariantBasic>::add_sector(const std::string& name) {
    Sector<VariantBasic>* sector = model->find_sector(name);
    if (sector == nullptr) {
        sector = new Sector<VariantBasic>(model, name, model->sectors_C.size(), get_named_property("sector", name, "upper_storage_limit").template as<Ratio>(),
                                          get_named_property("sector", name, "initial_storage_fill_factor").template as<FloatType>() * model->delta_t());
        model->sectors_C.emplace_back(sector);
    }
    return sector;
}

template<>
Sector<VariantDemand>* ModelInitializer<VariantDemand>::add_sector(const std::string& name) {
    Sector<VariantDemand>* sector = model->find_sector(name);
    if (sector == nullptr) {
        sector = new Sector<VariantDemand>(model, name, model->sectors_C.size(), get_named_property("sector", name, "upper_storage_limit").template as<Ratio>(),
                                           get_named_property("sector", name, "initial_storage_fill_factor").template as<FloatType>() * model->delta_t());
        sector->parameters_writable().storage_refill_enforcement_gamma =
            get_named_property("sector", name, "storage_refill_enforcement").template as<FloatType>() * model->delta_t();
        model->sectors_C.emplace_back(sector);
    }
    return sector;
}

template<>
Sector<VariantPrices>* ModelInitializer<VariantPrices>::add_sector(const std::string& name) {
    Sector<VariantPrices>* sector = model->find_sector(name);
    if (sector == nullptr) {
        sector = new Sector<VariantPrices>(model, name, model->sectors_C.size(), get_named_property("sector", name, "upper_storage_limit").template as<Ratio>(),
                                           get_named_property("sector", name, "initial_storage_fill_factor").template as<FloatType>() * model->delta_t());
        sector->parameters_writable().supply_elasticity = get_named_property("sector", name, "supply_elasticity").template as<Ratio>();
        sector->parameters_writable().price_increase_production_extension =
            get_named_property("sector", name, "price_increase_production_extension").template as<Price>();
        sector->parameters_writable().estimated_price_increase_production_extension =
            get_named_property("sector", name, "estimated_price_increase_production_extension")
                .template as<Price>(to_float(sector->parameters_writable().price_increase_production_extension));
        sector->parameters_writable().initial_markup = get_named_property("sector", name, "initial_markup").template as<Price>();
        sector->parameters_writable().consumption_price_elasticity = get_named_property("sector", name, "consumption_price_elasticity").template as<Ratio>();
        sector->parameters_writable().target_storage_refill_time =
            get_named_property("sector", name, "target_storage_refill_time").template as<FloatType>() * model->delta_t();
        sector->parameters_writable().target_storage_withdraw_time =
            get_named_property("sector", name, "target_storage_withdraw_time").template as<FloatType>() * model->delta_t();
        model->sectors_C.emplace_back(sector);
    }
    return sector;
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::initialize_connection(
    Sector<ModelVariant>* sector_from, Region<ModelVariant>* region_from, Sector<ModelVariant>* sector_to, Region<ModelVariant>* region_to, const Flow& flow) {
    Firm<ModelVariant>* firm_from = model->find_firm(sector_from, *region_from);
    if (!firm_from) {
        firm_from = add_firm(sector_from, region_from);
        if (!firm_from) {
            return;
        }
    }
    Firm<ModelVariant>* firm_to = model->find_firm(sector_to, *region_to);
    if (!firm_to) {
        firm_to = add_firm(sector_to, region_to);
        if (!firm_to) {
            return;
        }
    }
    initialize_connection(firm_from, firm_to, flow);
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::initialize_connection(Firm<ModelVariant>* firm_from, EconomicAgent<ModelVariant>* economic_agent_to, const Flow& flow) {
    if (model->no_self_supply() && (static_cast<void*>(firm_from) == static_cast<void*>(economic_agent_to))) {
        return;
    }
#ifdef DEBUG
#define SKIP(a)                                                                    \
    if (std::string(*firm_from) + "->" + std::string(*economic_agent_to) == (a)) { \
        warning("skipping " << (a));                                               \
        return;                                                                    \
    }
// use SKIP("...:...->...:..."); to skip connections
#endif
    Sector<ModelVariant>* sector_from = firm_from->sector;
    Storage<ModelVariant>* input_storage = economic_agent_to->find_input_storage(*sector_from);
    if (!input_storage) {
        input_storage = new Storage<ModelVariant>(sector_from, economic_agent_to);
        economic_agent_to->input_storages.emplace_back(input_storage);
    }
    assert(flow.get_quantity() > 0.0);
    input_storage->add_initial_flow_Z_star(flow);
    firm_from->add_initial_production_X_star(flow);

    auto business_connection = new BusinessConnection<ModelVariant>(input_storage->purchasing_manager.get(), firm_from->sales_manager.get(), flow);
    input_storage->purchasing_manager->business_connections.push_back(business_connection);

    if (static_cast<void*>(firm_from) == static_cast<void*>(economic_agent_to)) {
        firm_from->self_supply_connection(business_connection);
    }
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::clean_network() {
    int firm_count;
    int consumer_count;
    bool needs_cleaning = true;
    while (needs_cleaning) {
        firm_count = 0;
        consumer_count = 0;
        needs_cleaning = false;
#ifdef CLEANUP_INFO
        info("Cleaning up...");
#endif
        for (auto region = model->regions_R.begin(); region != model->regions_R.end(); region++) {
            for (auto economic_agent = (*region)->economic_agents.begin(); economic_agent != (*region)->economic_agents.end();) {
                if ((*economic_agent)->type == EconomicAgent<ModelVariant>::Type::FIRM) {
                    Firm<ModelVariant>* firm = (*economic_agent)->as_firm();

                    FlowQuantity input = FlowQuantity(0.0);
                    for (const auto& input_storage : firm->input_storages) {
                        input += input_storage->initial_input_flow_I_star().get_quantity();
                    }
                    FloatType value_added = to_float(firm->initial_production_X_star().get_quantity() - input);

                    if (value_added <= 0.0 || firm->sales_manager->business_connections.empty()
                        || (firm->sales_manager->business_connections.size() == 1 && firm->self_supply_connection()) || firm->input_storages.empty()
                        || (firm->input_storages.size() == 1 && firm->self_supply_connection())) {
                        needs_cleaning = true;

#ifdef CLEANUP_INFO
                        if (value_added <= 0.0) {
                            warning(std::string(*firm) << ": removed (value added only " << value_added << ")");
                        } else if (firm->sales_manager->business_connections.size() == 0
                                   || (firm->sales_manager->business_connections.size() == 1 && firm->self_supply_connection())) {
                            warning(std::string(*firm) << ": removed (no outgoing connection)");
                        } else {
                            warning(std::string(*firm) << ": removed (no incoming connection)");
                        }
#endif
                        firm->sector->remove_firm(firm);
                        // Alter initial_input_flow of buying economic agents
                        for (auto& business_connection : firm->sales_manager->business_connections) {
                            if (!business_connection->buyer->storage->subtract_initial_flow_Z_star(business_connection->initial_flow_Z_star())) {
                                business_connection->buyer->remove_business_connection(business_connection.get());
                            }
                        }

                        // Alter initial_production of supplying firms
                        for (auto& storage : firm->input_storages) {
                            for (auto& business_connection : storage->purchasing_manager->business_connections) {
                                business_connection->seller->firm->subtract_initial_production_X_star(business_connection->initial_flow_Z_star());
                                business_connection->seller->remove_business_connection(business_connection);
                            }
                        }

                        // Also cleans up memory of firm
                        economic_agent = (*region)->economic_agents.erase(economic_agent);
                    } else {
                        economic_agent++;
                        firm_count++;
                    }
                } else if ((*economic_agent)->type == EconomicAgent<ModelVariant>::Type::CONSUMER) {
                    Consumer<ModelVariant>* consumer = (*economic_agent)->as_consumer();

                    if (consumer->input_storages.empty()) {
#ifdef CLEANUP_INFO
                        warning(std::string(*consumer) << ": removed (no incoming connection)");
#endif
                        // Also cleans up memory of consumer
                        economic_agent = (*region)->economic_agents.erase(economic_agent);
                    } else {
                        economic_agent++;
                        consumer_count++;
                    }
                } else {
                    error("Unknown economic agent type");
                }
            }
        }
    }
    info("Number of firms: " << firm_count);
    info("Number of consumers: " << consumer_count);
    if (firm_count == 0 && consumer_count == 0) {
        error("No economic agents present");
    }
}

#ifdef DEBUG
template<class ModelVariant>
void ModelInitializer<ModelVariant>::print_network_characteristics() const {
    FloatType average_transport_delay = 0;
    unsigned int region_wo_firm_count = 0;
    for (auto& region : model->regions_R) {
        FloatType average_transport_delay_region = 0;
        unsigned int firm_count = 0;
        for (auto& economic_agent : region->economic_agents) {
            if (economic_agent->type == EconomicAgent<ModelVariant>::Type::FIRM) {
                FloatType average_tranport_delay_economic_agent = 0;
                Firm<ModelVariant>* firm = economic_agent->as_firm();
                for (auto& business_connection : firm->sales_manager->business_connections) {
                    average_tranport_delay_economic_agent += business_connection->get_transport_delay_tau();
                }
                assert(!economic_agent->as_firm()->sales_manager->business_connections.empty());
                average_tranport_delay_economic_agent /= FloatType(firm->sales_manager->business_connections.size());
                firm_count++;
                average_transport_delay_region += average_tranport_delay_economic_agent;
            }
        }
        if (firm_count > 0) {
            average_transport_delay_region /= FloatType(firm_count);
#ifdef CLEANUP_INFO
            info(std::string(*region) << ": number of firms: " << firm_count << " average transport delay: " << average_transport_delay_region);
#endif
            average_transport_delay += average_transport_delay_region;
        } else {
            region_wo_firm_count++;
            warning(std::string(*region) << ": no firm");
        }
    }
    average_transport_delay /= FloatType(model->regions_R.size() - region_wo_firm_count);
#ifdef CLEANUP_INFO
    info("Average transport delay: " << average_transport_delay);
#endif
}
#endif

template<class ModelVariant>
void ModelInitializer<ModelVariant>::read_centroids_netcdf(const std::string& filename) {
    {
        netCDF::NcFile file(filename, netCDF::NcFile::read);

        std::size_t regions_count = file.getDim("region").getSize();

        netCDF::NcVar regions_var = file.getVar("region");
        std::vector<const char*> regions_val(regions_count);
        regions_var.getVar(&regions_val[0]);

        netCDF::NcVar lons_var = file.getVar("lon");
        std::vector<float> lons_val(regions_count);
        lons_var.getVar(&lons_val[0]);

        netCDF::NcVar lats_var = file.getVar("lat");
        std::vector<float> lats_val(regions_count);
        lats_var.getVar(&lats_val[0]);

        for (std::size_t i = 0; i < regions_count; i++) {
            region_centroids.emplace(regions_val[i], GeographicPoint("centroid", lons_val[i], lats_val[i]));
        }
        file.close();
    }

    const settings::SettingsNode& transport = settings["transport"];
    threshold_road_transport = transport["threshold_road_transport"].as<FloatType>();
    avg_road_speed = transport["avg_road_speed"].as<FloatType>();
    avg_water_speed = transport["avg_water_speed"].as<FloatType>();
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::read_transport_times_csv(const std::string& index_filename, const std::string& filename) {
    std::vector<Region<ModelVariant>*> regions;
    std::ifstream index_file(index_filename.c_str());
    if (!index_file) {
        error("Could not open index file '" << index_filename << "'");
    }
    unsigned int index = 0;
    while (true) {
        std::string line;
        if (!getline(index_file, line)) {
            break;
        }
        if (line.empty() || (line.length() == 1 && line[0] == '\r')) {
            continue;
        }
        std::istringstream ss(line);

        std::string region_name;
        if (!getline(ss, region_name, ',')) {
            error("Unexpected end in index file");
        }
        if (!region_name.empty() && region_name[region_name.size() - 1] == '\r') {
            region_name.erase(region_name.size() - 1);
        }

        Region<ModelVariant>* region = model->find_region(region_name);
        if (!region) {
            region = new Region<ModelVariant>(model, region_name, model->regions_R.size());
            model->regions_R.emplace_back(region);
        }

        regions.push_back(region);
        index++;
    }

    std::ifstream transport_delays_file(filename.c_str());
    if (!transport_delays_file) {
        error("Could not open transport delays file '" << filename << "'");
    }

    TransportDelay transport_delay_tau = 1;
    std::string transport_line;

    for (std::size_t row = 0; row < regions.size(); row++) {
        if (!getline(transport_delays_file, transport_line)) {
            error("Index and transport_delays are not consistent: Not enough rows");
        }
        if (transport_line.empty() || (transport_line.length() == 1 && transport_line[0] == '\r')) {
            row--;
            continue;
        }

        std::istringstream transport_string_stream(transport_line);
        std::string transport_str;

        for (std::size_t col = 0; col < regions.size(); col++) {
            if (!getline(transport_string_stream, transport_str, ',')) {
                error("Index and transport_delays are not consistent: Not enough columns in row " << row);
            }
            if (!transport_str.empty() && transport_str[transport_str.size() - 1] == '\r') {
                transport_str.erase(transport_str.size() - 1);
            }
            transport_delay_tau = std::stoi(transport_str);
            if (transport_delay_tau <= 0 || transport_delay_tau > 254) {
                error("Transport delay not valid: " << transport_delay_tau << " in col " << col << " in row " << row);
            }
            auto inf = new Infrastructure<ModelVariant>(transport_delay_tau);
            regions[row]->connections.push_back(inf);
            regions[col]->connections.push_back(inf);
            inf->connections.push_back(regions[row]);
            inf->connections.push_back(regions[col]);
            model->infrastructure_G.emplace_back(inf);
            Path<ModelVariant> path = {
                transport_delay_tau,  // distance
                inf                   // infrastructure
            };
            regions[row]->paths.emplace(regions[col], path);
        }
    }
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::build_artificial_network() {
    const settings::SettingsNode& network = settings["network"];
    transport_time_base = TransportTimeBase::CONST;
    transport_time = network["transport"].as<unsigned int>(1);
    const auto closed = network["closed"].as<bool>(false);
    const auto skewness = network["skewness"].as<unsigned int>();
    if (skewness < 1) {
        error("Skewness must be >= 1");
    }
    const auto regions_cnt = network["regions"].as<unsigned int>();
    const auto sectors_cnt = network["sectors"].as<unsigned int>();
    for (std::size_t i = 0; i < sectors_cnt; i++) {
        add_sector("SEC" + std::to_string(i + 1));
    }
    for (std::size_t i = 0; i < regions_cnt; i++) {
        Region<ModelVariant>* region = add_region("RG" + std::to_string(i));
        add_consumer(region);
        for (std::size_t i = 0; i < sectors_cnt; i++) {
            add_firm(model->sectors_C[i + 1].get(), region);
        }
    }
    const Flow flow = Flow(FlowQuantity(1.0), Price(1.0));
    const Flow double_flow = Flow(FlowQuantity(2.0), Price(1.0));
    for (std::size_t r = 0; r < regions_cnt; r++) {
        for (std::size_t i = 0; i < sectors_cnt; i++) {
            info(std::string(*model->sectors_C[i + 1]->firms_N[r])
                 << "->" << std::string(*model->sectors_C[(i + 1) % sectors_cnt + 1]->firms_N[r]) << " = " << flow.get_quantity());
            initialize_connection(model->sectors_C[i + 1]->firms_N[r], model->sectors_C[(i + 1) % sectors_cnt + 1]->firms_N[r], flow);
            if (!closed && r == regions_cnt - 1) {
                info(std::string(*model->sectors_C[i + 1]->firms_N[r])
                     << "->" << std::string(*model->find_consumer(model->regions_R[r].get())) << " = " << double_flow.get_quantity());
                initialize_connection(model->sectors_C[i + 1]->firms_N[r], model->find_consumer(model->regions_R[r].get()), double_flow);
            } else {
                info(std::string(*model->sectors_C[i + 1]->firms_N[r])
                     << "->" << std::string(*model->find_consumer(model->regions_R[r].get())) << " = " << flow.get_quantity());
                initialize_connection(model->sectors_C[i + 1]->firms_N[r], model->find_consumer(model->regions_R[r].get()), flow);
            }
            if (closed || r < regions_cnt - 1) {
                info(std::string(*model->sectors_C[i + 1]->firms_N[r])
                     << "->" << std::string(*model->sectors_C[(i + skewness) % sectors_cnt + 1]->firms_N[(r + 1) % regions_cnt]) << " = "
                     << flow.get_quantity());
                initialize_connection(model->sectors_C[i + 1]->firms_N[r], model->sectors_C[(i + skewness) % sectors_cnt + 1]->firms_N[(r + 1) % regions_cnt],
                                      flow);
            }
        }
    }
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::read_flows() {
    const settings::SettingsNode& network = settings["network"];
    const auto flow_threshold = network["threshold"].as<FloatType>();
    mrio::Table<FloatType, std::size_t> table;
    const std::string& type = network["type"].as<std::string>();
    std::string filename;
    if (type == "csv") {
        read_transport();
        std::string index_filename;
        filename = network["file"].as<std::string>();
        index_filename = network["index"].as<std::string>();
        std::ifstream index_file(index_filename.c_str(), std::ios::in | std::ios::binary);
        if (!index_file) {
            error("Could not open index file '" << index_filename << "'");
        }
        std::ifstream flows_file(filename.c_str(), std::ios::in | std::ios::binary);
        if (!flows_file) {
            error("Could not open flows file '" << filename << "'");
        }
        table.read_from_csv(index_file, flows_file, flow_threshold);
        flows_file.close();
        index_file.close();
    } else if (type == "mrio") {
        read_transport();
        filename = network["file"].as<std::string>();
        std::ifstream flows_file(filename.c_str(), std::ios::in | std::ios::binary);
        if (!flows_file) {
            error("Could not open flows file '" << filename << "'");
        }
        table.read_from_mrio(flows_file, flow_threshold);
        flows_file.close();
    } else if (type == "netcdf") {
        read_transport();
        filename = network["file"].as<std::string>();
        table.read_from_netcdf(filename, flow_threshold);
    }

    std::vector<EconomicAgent<ModelVariant>*> economic_agents;
    economic_agents.reserve(table.index_set().size());

    for (const auto& index : table.index_set().total_indices) {
        const std::string& region_name = index.region->name;
        const std::string& sector_name = index.sector->name;
        Region<ModelVariant>* region = add_region(region_name);
        if (sector_name == "FCON") {
            Consumer<ModelVariant>* consumer = model->find_consumer(region);
            if (!consumer) {
                consumer = add_consumer(region);
                economic_agents.push_back(consumer);
            } else {
                error("Duplicate consumer for region " << region_name);
            }
        } else {
            Sector<ModelVariant>* sector = add_sector(sector_name);
            Firm<ModelVariant>* firm = model->find_firm(sector, *region);
            if (!firm) {
                firm = add_firm(sector, region);
                if (!firm) {
                    error("Could not add firm");
                }
            }
            economic_agents.push_back(firm);
        }
    }

    const Ratio time_factor = model->delta_t() / Time(365.0);
    const FlowQuantity daily_flow_threshold = round(FlowQuantity(flow_threshold * time_factor));
    auto d = table.raw_data().begin();
    for (auto& source : economic_agents) {
        if (source->type == EconomicAgent<ModelVariant>::Type::FIRM) {
            Firm<ModelVariant>* firm_from = source->as_firm();
            for (auto& target : economic_agents) {
                const FlowQuantity flow_quantity = round(FlowQuantity(*d) * time_factor);
                if (flow_quantity > daily_flow_threshold) {
                    initialize_connection(firm_from, target, flow_quantity);
                }
                d++;
            }
        } else {
            d += table.index_set().size();
        }
    }
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::read_transport() {
    const settings::SettingsNode& transport = settings["transport"];
    const std::string& type = transport["type"].as<std::string>();
    if (type == "csv") {
        transport_time_base = TransportTimeBase::PRECALCULATED;
        read_transport_times_csv(transport["index"].as<std::string>(), transport["file"].as<std::string>());
    } else if (type == "const") {
        transport_time_base = TransportTimeBase::CONST;
        transport_time = transport["value"].as<unsigned int>();
    } else if (type == "centroids") {
        transport_time_base = TransportTimeBase::CENTROIDS;
        read_centroids_netcdf(transport["file"].as<std::string>());
    } else {
        error("no valid transport times given");
    }
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::initialize() {
    pre_initialize_variant();
    const std::string& type = settings["network"]["type"].template as<std::string>();
    if (type == "csv" || type == "mrio" || type == "netcdf") {
        read_transport();
        read_flows();
    } else if (type == "artificial") {
        build_artificial_network();
    } else {
        error("Unknown network type '" << type << "'");
    }
    clean_network();
    post_initialize_variant();
}

template<>
void ModelInitializer<VariantBasic>::pre_initialize_variant() {}

template<>
void ModelInitializer<VariantDemand>::pre_initialize_variant() {
    const settings::SettingsNode& parameters = settings["parameters"];
    model->parameters_writable().history_weight = parameters["history_weight"].as<Ratio>();
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::pre_initialize_variant() {
    const settings::SettingsNode& parameters = settings["parameters"];
    model->parameters_writable().transport_penalty_small = parameters["transport_penalty_small"].as<Price>();
    model->parameters_writable().transport_penalty_large = parameters["transport_penalty_large"].as<Price>();
    model->parameters_writable().storage_penalty_small = parameters["storage_penalty_small"].as<FloatType>();
    model->parameters_writable().storage_penalty_large = parameters["storage_penalty_large"].as<FloatType>();
    model->parameters_writable().optimization_maxiter = parameters["optimization_maxiter"].as<unsigned int>();
    model->parameters_writable().optimization_timeout = parameters["optimization_timeout"].as<unsigned int>();
    model->parameters_writable().quadratic_storage_penalty = parameters["quadratic_storage_penalty"].as<bool>();
    model->parameters_writable().quadratic_transport_penalty = parameters["quadratic_transport_penalty"].as<bool>();
    model->parameters_writable().maximal_decrease_reservation_price_limited_by_markup =
        parameters["maximal_decrease_reservation_price_limited_by_markup"].as<bool>();
    model->parameters_writable().always_extend_expected_demand_curve = parameters["always_extend_expected_demand_curve"].as<bool>();
    model->parameters_writable().naive_expectations = parameters["naive_expectations"].as<bool>();
    model->parameters_writable().deviation_penalty = parameters["deviation_penalty"].as<bool>(false);
    model->parameters_writable().cost_correction = parameters["cost_correction"].as<bool>();
    model->parameters_writable().min_storage = parameters["min_storage"].as<Ratio>(0.0);
    model->parameters_writable().cheapest_price_range_preserve_seller_price = parameters["cheapest_price_range_preserve_seller_price"].as<bool>(false);
    model->parameters_writable().cheapest_price_range_generic_size = (parameters["cheapest_price_range_width"].as<std::string>() == "auto");
    if (!model->parameters_writable().cheapest_price_range_generic_size) {
        model->parameters_writable().cheapest_price_range_width = parameters["cheapest_price_range_width"].as<Price>();
    }
    model->parameters_writable().relative_transport_penalty = parameters["relative_transport_penalty"].as<bool>();
    const std::string& optimization_algorithm = parameters["optimization_algorithm"].as<std::string>("slsqp");
    if (optimization_algorithm == "slsqp") {
        model->parameters_writable().optimization_algorithm = nlopt::LD_SLSQP;
    } else if (optimization_algorithm == "mma") {
        model->parameters_writable().optimization_algorithm = nlopt::LD_MMA;
    } else if (optimization_algorithm == "ccsaq") {
        model->parameters_writable().optimization_algorithm = nlopt::LD_CCSAQ;
    } else if (optimization_algorithm == "lbfgs") {
        model->parameters_writable().optimization_algorithm = nlopt::LD_LBFGS;
    } else if (optimization_algorithm == "tnewton_precond_restart") {
        model->parameters_writable().optimization_algorithm = nlopt::LD_TNEWTON_PRECOND_RESTART;
    } else if (optimization_algorithm == "tnewton_precond") {
        model->parameters_writable().optimization_algorithm = nlopt::LD_TNEWTON_PRECOND;
    } else if (optimization_algorithm == "tnewton_restart") {
        model->parameters_writable().optimization_algorithm = nlopt::LD_TNEWTON_RESTART;
    } else if (optimization_algorithm == "tnewton") {
        model->parameters_writable().optimization_algorithm = nlopt::LD_TNEWTON;
    } else if (optimization_algorithm == "var1") {
        model->parameters_writable().optimization_algorithm = nlopt::LD_VAR1;
    } else if (optimization_algorithm == "var2") {
        model->parameters_writable().optimization_algorithm = nlopt::LD_VAR2;
    } else {
        error("unknown optimization alorithm '" << optimization_algorithm << "'");
    }
}

template<>
void ModelInitializer<VariantBasic>::post_initialize_variant() {}

template<>
void ModelInitializer<VariantDemand>::post_initialize_variant() {}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::post_initialize_variant() {
    // initialize price dependent members of each capacityManager, which can only be calculated after the whole network has been initialized
    for (auto& sector : model->sectors_C) {
        for (auto& firm : sector->firms_N) {
            firm->sales_manager->initialize();
        }
    }
}

INSTANTIATE_BASIC(ModelInitializer);
}  // namespace acclimate
