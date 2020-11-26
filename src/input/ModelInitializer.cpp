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

#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <numeric>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "acclimate.h"
#include "model/BusinessConnection.h"
#include "model/Consumer.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/GeoConnection.h"
#include "model/GeoEntity.h"
#include "model/GeoLocation.h"
#include "model/GeoPoint.h"
#include "model/GeoRoute.h"
#include "model/Model.h"
#include "model/PurchasingManager.h"
#include "model/Region.h"
#include "model/SalesManager.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "netcdfpp.h"
#include "optimization.h"
#include "parameters.h"

namespace acclimate {

ModelInitializer::ModelInitializer(Model* model_p, const settings::SettingsNode& settings_p) : model_m(model_p), settings(settings_p) {
    const settings::SettingsNode& parameters = settings["model"];
    const settings::SettingsNode& run = settings["run"];
    model()->set_start_time(run["start"].as<Time>());
    model()->set_stop_time(run["stop"].as<Time>());
    model()->set_delta_t(parameters["delta_t"].as<Time>());
    model()->no_self_supply(parameters["no_self_supply"].as<bool>());
}

settings::SettingsNode ModelInitializer::get_named_property(const settings::SettingsNode& node_settings,
                                                            const std::string& node_name,
                                                            const std::string& property_name) const {
    if (node_settings.has(node_name) && node_settings[node_name].has(property_name)) {
        return node_settings[node_name][property_name];
    }
    return node_settings["ALL"][property_name];
}

settings::SettingsNode ModelInitializer::get_firm_property(const std::string& name,
                                                           const std::string& sector_name,
                                                           const std::string& region_name,
                                                           const std::string& property_name) const {
    const settings::SettingsNode& firm_settings = settings["firms"];

    if (firm_settings.has(name) && firm_settings[name].has(property_name)) {
        return firm_settings[name][property_name];
    }
    if (firm_settings.has(sector_name + ":" + region_name) && firm_settings[sector_name + ":" + region_name].has(property_name)) {
        return firm_settings[sector_name + ":" + region_name][property_name];
    }
    if (firm_settings.has(sector_name) && firm_settings[sector_name].has(property_name)) {
        return firm_settings[sector_name][property_name];
    }
    if (firm_settings.has(region_name) && firm_settings[region_name].has(property_name)) {
        return firm_settings[region_name][property_name];
    }
    return firm_settings["ALL"][property_name];
}

Firm* ModelInitializer::add_firm(std::string name, Sector* sector, Region* region) {
    if (model()->economic_agents.find(name) != nullptr) {
        throw log::error(this, "Ambiguous agent name", name);
    }
    auto* firm = model()->economic_agents.add<Firm>(
        id_t(std::move(name)), sector, region,
        static_cast<Ratio>(get_firm_property(name, sector->name(), region->name(), "possible_overcapacity_ratio").as<FloatType>()));
    sector->firms.add(firm);
    return firm;
}

Consumer* ModelInitializer::add_consumer(std::string name, Region* region) {
    if (model()->economic_agents.find(name) != nullptr) {
        throw log::error(this, "Ambiguous agent name", name);
    }
    auto* consumer = model()->economic_agents.add<Consumer>(id_t(std::move(name)), region);
    region->economic_agents.add(consumer);
    return consumer;
}

EconomicAgent* ModelInitializer::add_standard_agent(Sector* sector, Region* region) {
    const std::string name = sector->name() + ":" + region->name();
    if (sector->name() == "FCON") {
        return add_consumer(std::move(name), region);
    } else {
        return add_firm(std::move(name), sector, region);
    }
}

Region* ModelInitializer::add_region(const std::string& name) {
    auto* region = model()->regions.add(model(), id_t(name));
    if (region->id.name != name) {
        throw log::error(this, "Ambiguous region name", name);
    }
    return region;
}

Sector* ModelInitializer::add_sector(const std::string& name) {
    Sector* sector = model()->sectors.find(name);
    if (sector == nullptr) {
        const settings::SettingsNode& sectors_node = settings["sectors"];
        sectors_node.require();
        sector = model()->sectors.add(model(), id_t(name), get_named_property(sectors_node, name, "upper_storage_limit").as<Ratio>(),
                                      get_named_property(sectors_node, name, "initial_storage_fill_factor").as<FloatType>() * model()->delta_t(),
                                      Sector::map_transport_type(get_named_property(sectors_node, name, "transport").as<hashed_string>()));
        sector->parameters_writable().supply_elasticity = get_named_property(sectors_node, name, "supply_elasticity").as<Ratio>();
        sector->parameters_writable().price_increase_production_extension =
            get_named_property(sectors_node, name, "price_increase_production_extension").as<Price>();
        sector->parameters_writable().estimated_price_increase_production_extension =
            get_named_property(sectors_node, name, "estimated_price_increase_production_extension")
                .as<Price>(to_float(sector->parameters_writable().price_increase_production_extension));
        sector->parameters_writable().initial_markup = get_named_property(sectors_node, name, "initial_markup").as<Price>();
        sector->parameters_writable().target_storage_refill_time =
            get_named_property(sectors_node, name, "target_storage_refill_time").as<FloatType>() * model()->delta_t();
        sector->parameters_writable().target_storage_withdraw_time =
            get_named_property(sectors_node, name, "target_storage_withdraw_time").as<FloatType>() * model()->delta_t();
    }
    if (sector->id.name != name) {
        throw log::error(this, "Ambiguous sector name", name);
    }
    return sector;
}

void ModelInitializer::initialize_connection(Firm* firm_from, EconomicAgent* economic_agent_to, const Flow& flow) {
    if (model()->no_self_supply() && (static_cast<void*>(firm_from) == static_cast<void*>(economic_agent_to))) {
        return;
    }
    auto sector_from = firm_from->sector;
    auto input_storage = economic_agent_to->input_storages.find(sector_from->name());
    if (input_storage == nullptr) {
        input_storage = economic_agent_to->input_storages.add(sector_from, economic_agent_to);
        if (economic_agent_to->is_consumer()) {
            const settings::SettingsNode& consumers_node = settings["consumers"];
            consumers_node.require();
            input_storage->parameters_writable().consumption_price_elasticity =
                get_named_property(consumers_node, sector_from->name() + "->" + economic_agent_to->region->name(), "consumption_price_elasticity").as<Ratio>();
        }
    }
    assert(flow.get_quantity() > 0.0);
    input_storage->add_initial_flow_Z_star(flow);
    firm_from->add_initial_production_X_star(flow);

    auto business_connection = std::make_shared<BusinessConnection>(input_storage->purchasing_manager.get(), firm_from->sales_manager.get(), flow);
    firm_from->sales_manager->business_connections.emplace_back(business_connection);
    input_storage->purchasing_manager->business_connections.emplace_back(business_connection);

    if (static_cast<void*>(firm_from) == static_cast<void*>(economic_agent_to)) {
        firm_from->self_supply_connection(business_connection);
    }
}

void ModelInitializer::clean_network() {
    std::size_t firm_count;
    std::size_t consumer_count;
    while (true) {
        firm_count = 0;
        consumer_count = 0;
        if constexpr (options::CLEANUP_INFO) {
            log::info(this, "Cleaning up...");
        }
        std::vector<EconomicAgent*> to_remove;
        for (auto& economic_agent : model()->economic_agents) {
            switch (economic_agent->type) {
                case EconomicAgent::type_t::FIRM: {
                    Firm* firm = economic_agent->as_firm();

                    auto input = std::accumulate(std::begin(firm->input_storages), std::end(firm->input_storages), FlowQuantity(0.0),
                                                 [](FlowQuantity q, const auto& is) { return std::move(q) + is->initial_input_flow_I_star().get_quantity(); });
                    FloatType value_added = to_float(firm->initial_production_X_star().get_quantity() - input);

                    if (value_added <= 0.0 || firm->sales_manager->business_connections.empty()
                        || (firm->sales_manager->business_connections.size() == 1 && firm->self_supply_connection() != nullptr) || firm->input_storages.empty()
                        || (firm->input_storages.size() == 1 && firm->self_supply_connection() != nullptr)) {
                        if constexpr (options::CLEANUP_INFO) {
                            if (value_added <= 0.0) {
                                log::warning(this, firm->name(), ": removed (value added only ", value_added, ")");
                            } else if (firm->sales_manager->business_connections.empty()
                                       || (firm->sales_manager->business_connections.size() == 1 && firm->self_supply_connection() != nullptr)) {
                                log::warning(this, firm->name(), ": removed (no outgoing connection)");
                            } else {
                                log::warning(this, firm->name(), ": removed (no incoming connection)");
                            }
                        }
                        // Alter initial_input_flow of buying economic agents
                        for (auto& business_connection : firm->sales_manager->business_connections) {
                            if (business_connection->buyer == nullptr) {
                                throw log::error(this, "Buyer invalid");
                            }
                            if (!business_connection->buyer->storage->subtract_initial_flow_Z_star(business_connection->initial_flow_Z_star())) {
                                business_connection->buyer->remove_business_connection(business_connection.get());
                            }
                        }

                        // Alter initial_production of supplying firms
                        for (auto& storage : firm->input_storages) {
                            for (auto& business_connection : storage->purchasing_manager->business_connections) {
                                if (business_connection->seller == nullptr) {
                                    throw log::error(this, "Seller invalid");
                                }
                                business_connection->seller->firm->subtract_initial_production_X_star(business_connection->initial_flow_Z_star());
                                business_connection->seller->remove_business_connection(business_connection.get());
                            }
                        }

                        firm->sector->firms.remove(firm);
                        firm->region->economic_agents.remove(economic_agent.get());
                        to_remove.push_back(economic_agent.get());
                    } else {
                        ++firm_count;
                    }
                } break;
                case EconomicAgent::type_t::CONSUMER: {
                    Consumer* consumer = economic_agent->as_consumer();
                    if (consumer->input_storages.empty()) {
                        if constexpr (options::CLEANUP_INFO) {
                            log::warning(this, consumer->name(), ": removed (no incoming connection)");
                        }
                        consumer->region->economic_agents.remove(economic_agent.get());
                        to_remove.push_back(economic_agent.get());
                    } else {
                        ++consumer_count;
                    }
                } break;
            }
        }
        if (to_remove.empty()) {
            break;
        }
        model()->economic_agents.remove(to_remove);
    }
    log::info(this, "Number of firms: ", firm_count);
    log::info(this, "Number of consumers: ", consumer_count);
    if (firm_count == 0 && consumer_count == 0) {
        throw log::error(this, "No economic agents present");
    }
}

void ModelInitializer::debug_print_network_characteristics() const {
    if constexpr (options::DEBUGGING) {
        FloatType average_transport_delay = 0;
        unsigned int region_wo_firm_count = 0;
        for (const auto& region : model()->regions) {
            FloatType average_transport_delay_region = 0;
            unsigned int firm_count = 0;
            for (const auto& economic_agent : region->economic_agents) {
                if (economic_agent->type == EconomicAgent::type_t::FIRM) {
                    FloatType average_tranport_delay_economic_agent = 0;
                    const auto* firm = economic_agent->as_firm();
                    for (const auto& business_connection : firm->sales_manager->business_connections) {
                        average_tranport_delay_economic_agent += business_connection->get_transport_delay_tau();
                    }
                    assert(!economic_agent->as_firm()->sales_manager->business_connections.empty());
                    average_tranport_delay_economic_agent /= FloatType(firm->sales_manager->business_connections.size());
                    ++firm_count;
                    average_transport_delay_region += average_tranport_delay_economic_agent;
                }
            }
            if (firm_count > 0) {
                average_transport_delay_region /= FloatType(firm_count);
                if constexpr (options::CLEANUP_INFO) {
                    log::info(this, region->name(), ": number of firms: ", firm_count, " average transport delay: ", average_transport_delay_region);
                    average_transport_delay += average_transport_delay_region;
                }
            } else {
                ++region_wo_firm_count;
                log::warning(this, region->name(), ": no firm");
            }
        }
        if constexpr (options::CLEANUP_INFO) {
            average_transport_delay /= FloatType(model()->regions.size() - region_wo_firm_count);
            log::info(this, "Average transport delay: ", average_transport_delay);
        }
    }
}

void ModelInitializer::read_transport_network_netcdf(const std::string& filename) {
    const settings::SettingsNode& transport = settings["transport"];
    const auto aviation_speed = transport["aviation_speed"].as<FloatType>();
    const auto road_speed = transport["road_speed"].as<FloatType>();
    const auto sea_speed = transport["sea_speed"].as<FloatType>();
    const auto port_delay = transport["port_delay"].as<TransportDelay>();
    const auto road_km_costs = transport["road_km_costs"].as<FloatType>();
    const auto sea_km_costs = transport["sea_km_costs"].as<FloatType>();

    netCDF::File file(filename, 'r');

    const auto typenames = file.variable("typeindex").require().require_dimensions({"typeindex"}).get<std::string>();

    char type_port = -1;
    char type_region = -1;
    char type_sea = -1;
    for (std::size_t i = 0; i < typenames.size(); ++i) {
        const auto& s = typenames[i];
        if (s == "port") {
            type_port = i;
        } else if (s == "sea") {
            type_sea = i;
        } else if (s == "region") {
            type_region = i;
        } else {
            throw log::error(this, "Unknown transport node type '", s, "'");
        }
    }
    if (type_port < 0) {
        throw log::error(this, "No transport node type for ports found");
    }
    if (type_region < 0) {
        throw log::error(this, "No transport node type for regions found");
    }
    if (type_sea < 0) {
        throw log::error(this, "No transport node type for seas found");
    }

    const auto file_index_count = file.dimension("index").require().size();
    const auto names = file.variable("index").require().require_dimensions({"index"}).get<std::string>();
    const auto types = file.variable("type").require().require_dimensions({"index"}).get<unsigned char>();
    const auto latitudes = file.variable("latitude").require().require_dimensions({"index"}).get<double>();
    const auto longitudes = file.variable("longitude").require().require_dimensions({"index"}).get<double>();
    const auto connections = file.variable("connections").require().require_dimensions({"index", "index"}).get<unsigned char>();

    file.close();

    class TemporaryGeoEntity {
      private:
        std::unique_ptr<GeoEntity> entity_m;

      public:
        bool used = false;
        std::size_t index = 0;

      public:
        explicit TemporaryGeoEntity(GeoEntity* entity_p) : entity_m(entity_p) {}
        ~TemporaryGeoEntity() {
            if (used) {
                (void)entity_m.release();
            }
        }
        GeoEntity* entity() { return entity_m.get(); }
    };

    std::vector<std::shared_ptr<TemporaryGeoEntity>> locations;

    for (std::size_t i = 0; i < names.size(); ++i) {
        GeoLocation* location = nullptr;
        if (types[i] == type_region) {
            location = model()->regions.find(names[i]);
            if (location == nullptr) {
                log::warning(this, "Geographic region '", names[i], "' not used by economy");
                continue;
            }
        } else if (types[i] == type_port) {
            location = new GeoLocation(model(), id_t(names[i]), port_delay, GeoLocation::type_t::PORT);
        } else if (types[i] == type_sea) {
            location = new GeoLocation(model(), id_t(names[i]), 0, GeoLocation::type_t::SEA);
        } else {
            throw log::error(this, "Invalid location type for '", names[i], "'");
        }
        location->set_centroid(longitudes[i], latitudes[i]);
        auto* tmp = new TemporaryGeoEntity(location);
        if (types[i] == type_region) {
            tmp->used = true;
        }
        locations.emplace_back(tmp);
        tmp->index = i;
    }

    class TemporaryGeoPath {
      private:
        FloatType costs_m = 0;
        std::vector<std::shared_ptr<TemporaryGeoEntity>> points_m;

      public:
        TemporaryGeoPath() = default;
        TemporaryGeoPath(FloatType costs_p,
                         std::shared_ptr<TemporaryGeoEntity> p1,
                         std::shared_ptr<TemporaryGeoEntity> p2,
                         std::shared_ptr<TemporaryGeoEntity> connection)
            : costs_m(costs_p), points_m({p1, connection, p2}) {}
        FloatType costs() const { return costs_m; }
        bool empty() const { return points_m.empty(); }
        const std::vector<std::shared_ptr<TemporaryGeoEntity>>& locations() const { return points_m; }
        TemporaryGeoPath operator+(const TemporaryGeoPath& other) const {
            TemporaryGeoPath res;
            if (empty()) {
                res.costs_m = other.costs_m;
                res.points_m.assign(std::begin(other.points_m), std::end(other.points_m));
            } else if (other.empty()) {
                res.costs_m = costs_m;
                res.points_m.assign(std::begin(points_m), std::end(points_m));
            } else {
                res.costs_m = costs_m + other.costs_m;
                res.points_m.assign(std::begin(points_m), std::end(points_m));
                res.points_m.resize(points_m.size() + other.points_m.size() - 1);
                std::copy(std::begin(other.points_m), std::end(other.points_m), std::begin(res.points_m) + points_m.size() - 1);
            }
            return res;
        }
    };

    // create direct connections
    const auto size = locations.size();
    std::vector<TemporaryGeoPath> paths(size * size);
    for (std::size_t i = 0; i < size; ++i) {
        auto& p1 = locations[i];
        auto* l1 = p1->entity()->as_location();
        // connection from location to itself only added for calrity when debugging:
        paths[i * size + i] =
            TemporaryGeoPath(0, p1, p1, std::make_shared<TemporaryGeoEntity>(new GeoConnection(model(), 0, GeoConnection::type_t::UNSPECIFIED, l1, l1)));
        for (std::size_t j = 0; j < i; ++j) {  // only go along subdiagonal
            auto& p2 = locations[j];
            if (connections[p1->index * file_index_count + p2->index] != connections[p2->index * file_index_count + p1->index]) {
                throw log::error(this, "Transport matrix is not symmetric");
            }
            if (connections[p1->index * file_index_count + p2->index] > 0) {
                auto* l2 = p2->entity()->as_location();
                FloatType costs;
                TransportDelay delay;
                GeoConnection::type_t type;
                const auto distance = l1->centroid()->distance_to(*l2->centroid());
                if (l1->type == GeoLocation::type_t::SEA || l2->type == GeoLocation::type_t::SEA) {
                    delay = iround(distance / sea_speed / 24. / to_float(model()->delta_t()));
                    type = GeoConnection::type_t::SEAROUTE;
                    costs = sea_km_costs * distance;
                } else {
                    delay = iround(distance / road_speed / 24. / to_float(model()->delta_t()));
                    type = GeoConnection::type_t::ROAD;
                    costs = road_km_costs * distance;
                }
                // connections is symmetric -> connection needs to be used twice to make sure it's the same object
                auto c = std::make_shared<TemporaryGeoEntity>(new GeoConnection(model(), delay, type, l1, l2));
                paths[i * size + j] = TemporaryGeoPath(costs, p1, p2, c);
                paths[j * size + i] = TemporaryGeoPath(costs, p2, p1, c);
            }
        }
    }

    // find cheapest connections
    bool done = false;
    while (!done) {
        log::info(this, "Find cheapest paths iteration...");
        done = true;
        for (std::size_t i = 0; i < size; ++i) {
            for (std::size_t j = 0; j < size; ++j) {
                auto* p = &paths[i * size + j];
                for (std::size_t via = 0; via < size; ++via) {
                    if (via == i || via == j) {
                        continue;
                    }
                    const auto& p1 = paths[i * size + via];
                    if (p1.empty()) {  // no connection from i to via
                        continue;
                    }
                    const auto& p2 = paths[via * size + j];
                    if (p2.empty()) {  // no connection from via to j
                        continue;
                    }
                    if (p->empty() || p1.costs() + p2.costs() < p->costs()) {
                        paths[i * size + j] = p1 + p2;
                        p = &paths[i * size + j];
                        done = false;
                    }
                }
            }
        }
    }

    // mark everything used
    bool found = true;
    while (found) {
        found = false;
        for (std::size_t i = 0; i < size; ++i) {
            auto& p1 = locations[i];
            if (p1->used) {  // regions are already marked used
                for (std::size_t j = 0; j < size; ++j) {
                    auto& p2 = locations[j];
                    if (p2->used) {  // regions are already marked used
                        auto& path = paths[i * size + j].locations();
                        if (path.empty()) {
                            throw log::error(this, "No roadsea transport connection from ", p1->entity()->as_location()->name(), " to ",
                                             p2->entity()->as_location()->name());
                        }
                        for (std::size_t k = 1; k < path.size() - 1; ++k) {
                            found = found || !path[k]->used;
                            path[k]->used = true;
                        }
                    }
                }
            }
        }
    }

    // add connections to locations actually used and create routes between regions
    for (std::size_t i = 0; i < size; ++i) {
        auto& p1 = locations[i];
        if (p1->used) {
            auto* l1 = p1->entity()->as_location();
            if (l1->type != GeoLocation::type_t::REGION) {
                model()->other_locations.add_raw(l1);
            }
            for (std::size_t j = 0; j < size; ++j) {
                auto& p2 = locations[j];
                if (p2->used) {
                    auto* l2 = p2->entity()->as_location();
                    auto& path = paths[i * size + j].locations();
                    if (!path.empty()) {
                        if (path.size() == 3 && i < j) {  // direct connection, only once per i/j combination
                            auto c = std::shared_ptr<GeoConnection>(path[1]->entity()->as_connection());
                            l1->connections.push_back(c);
                            l2->connections.push_back(c);
                        }
                        if (l1->type == GeoLocation::type_t::REGION && l2->type == GeoLocation::type_t::REGION) {
                            // create roadsea route
                            auto* r1 = l1->as_region();
                            auto* r2 = l2->as_region();
                            GeoRoute route;
                            route.path.reserve(path.size() - 2);
                            for (std::size_t k = 1; k < path.size() - 1; ++k) {
                                route.path.add(path[k]->entity());
                            }
                            r1->routes.emplace(std::make_pair(r2->id.index(), Sector::transport_type_t::ROADSEA), route);
                        }
                    }
                    if (l1->type == GeoLocation::type_t::REGION && l2->type == GeoLocation::type_t::REGION) {
                        // create aviation route
                        const auto distance = l1->centroid()->distance_to(*l2->centroid());
                        const auto delay = iround(distance / aviation_speed / 24. / to_float(model()->delta_t()));
                        auto c = std::make_shared<GeoConnection>(model(), delay, GeoConnection::type_t::AVIATION, l1, l2);
                        l1->connections.push_back(c);
                        l2->connections.push_back(c);
                        auto* r1 = l1->as_region();
                        auto* r2 = l2->as_region();
                        GeoRoute route;
                        route.path.add(c.get());
                        r1->routes.emplace(std::make_pair(r2->id.index(), Sector::transport_type_t::AVIATION), route);
                    }
                }
            }
        }
    }
}

void ModelInitializer::read_centroids_netcdf(const std::string& filename) {
    {
        netCDF::File file(filename, 'r');

        const auto regions = file.variable("region").require().require_dimensions({"region"}).get<std::string>();
        const auto lons = file.variable("lon").require().require_dimensions({"region"}).get<float>();
        const auto lats = file.variable("lat").require().require_dimensions({"region"}).get<float>();

        for (std::size_t i = 0; i < regions.size(); ++i) {
            Region* region = model()->regions.find(regions[i]);
            if (region != nullptr) {
                region->set_centroid(lons[i], lats[i]);
            }
        }
        file.close();
    }

    const settings::SettingsNode& transport = settings["transport"];
    const auto threshold_road_transport = transport["threshold_road_transport"].as<FloatType>();
    const auto aviation_speed = transport["aviation_speed"].as<FloatType>();
    const auto road_speed = transport["road_speed"].as<FloatType>();
    const auto sea_speed = transport["sea_speed"].as<FloatType>();

    for (auto& region_from : model()->regions) {
        if (region_from->centroid() == nullptr) {
            throw log::error(this, "Centroid for ", region_from->name(), " not found");
        }
        for (auto& region_to : model()->regions) {
            if (region_from == region_to) {
                break;
            }
            const auto distance = region_from->centroid()->distance_to(*region_to->centroid());
            // create roadsea route
            {
                TransportDelay transport_delay;
                GeoConnection::type_t type;
                if (distance >= threshold_road_transport) {
                    transport_delay = iround(distance / sea_speed / 24. / to_float(model()->delta_t()));
                    type = GeoConnection::type_t::SEAROUTE;
                } else {
                    transport_delay = iround(distance / road_speed / 24. / to_float(model()->delta_t()));
                    type = GeoConnection::type_t::ROAD;
                }
                if (transport_delay > 0) {
                    --transport_delay;
                }
                auto inf = std::make_shared<GeoConnection>(model(), transport_delay, type, region_from.get(), region_to.get());
                region_from->connections.push_back(inf);
                region_to->connections.push_back(inf);

                GeoRoute route;
                route.path.add(inf.get());
                region_from->routes.emplace(std::make_pair(region_to->id.index(), Sector::transport_type_t::ROADSEA), route);
                region_to->routes.emplace(std::make_pair(region_from->id.index(), Sector::transport_type_t::ROADSEA), route);
            }
            // create aviation route
            {
                TransportDelay transport_delay = iround(distance / aviation_speed / 24. / to_float(model()->delta_t()));
                if (transport_delay > 0) {
                    --transport_delay;
                }
                auto inf = std::make_shared<GeoConnection>(model(), transport_delay, GeoConnection::type_t::AVIATION, region_from.get(), region_to.get());
                region_from->connections.push_back(inf);
                region_to->connections.push_back(inf);

                GeoRoute route;
                route.path.add(inf.get());
                region_from->routes.emplace(std::make_pair(region_to->id.index(), Sector::transport_type_t::AVIATION), route);
                region_to->routes.emplace(std::make_pair(region_from->id.index(), Sector::transport_type_t::AVIATION), route);
            }
        }
    }
}

void ModelInitializer::create_simple_transport_connection(Region* region_from, Region* region_to, TransportDelay transport_delay) {
    auto inf = std::make_shared<GeoConnection>(model(), transport_delay, GeoConnection::type_t::UNSPECIFIED, region_from, region_to);
    region_from->connections.push_back(inf);
    region_to->connections.push_back(inf);

    GeoRoute route;
    route.path.add(inf.get());
    region_from->routes.emplace(std::make_pair(region_to->id.index(), Sector::transport_type_t::AVIATION), route);
    region_from->routes.emplace(std::make_pair(region_to->id.index(), Sector::transport_type_t::ROADSEA), route);
    region_to->routes.emplace(std::make_pair(region_from->id.index(), Sector::transport_type_t::AVIATION), route);
    region_to->routes.emplace(std::make_pair(region_from->id.index(), Sector::transport_type_t::ROADSEA), route);
}

void ModelInitializer::read_transport_times_csv(const std::string& index_filename, const std::string& filename) {
    std::vector<Region*> regions;
    std::ifstream index_file(index_filename.c_str());
    if (!index_file) {
        throw log::error(this, "Could not open index file '", index_filename, "'");
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
            throw log::error(this, "Unexpected end in index file");
        }
        if (!region_name.empty() && region_name[region_name.size() - 1] == '\r') {
            region_name.erase(region_name.size() - 1);
        }

        Region* region = model()->regions.find(region_name);
        regions.push_back(region);  // might be == nullptr
        ++index;
    }

    std::ifstream transport_delays_file(filename.c_str());
    if (!transport_delays_file) {
        throw log::error(this, "Could not open transport delays file '", filename, "'");
    }

    TransportDelay transport_delay_tau = 1;
    std::string transport_line;

    for (std::size_t row = 0; row < regions.size(); ++row) {
        if (!getline(transport_delays_file, transport_line)) {
            throw log::error(this, "Index and transport_delays are not consistent: Not enough rows");
        }
        if (transport_line.empty() || (transport_line.length() == 1 && transport_line[0] == '\r')) {
            --row;
            continue;
        }

        auto region_from = regions[row];
        if (region_from != nullptr) {
            std::istringstream transport_string_stream(transport_line);
            std::string transport_str;

            for (std::size_t col = 0; col < row; ++col) {
                if (!getline(transport_string_stream, transport_str, ',')) {
                    throw log::error(this, "Index and transport_delays are not consistent: Not enough columns in row ", row);
                }
                if (!transport_str.empty() && transport_str[transport_str.size() - 1] == '\r') {
                    transport_str.erase(transport_str.size() - 1);
                }

                auto region_to = regions[col];
                if (region_to != nullptr && region_from != region_to) {
                    transport_delay_tau = std::stoi(transport_str);
                    if (transport_delay_tau <= 0) {
                        throw log::error(this, "Transport delay not valid: ", transport_delay_tau, " in col ", col, " in row ", row);
                    }
                    create_simple_transport_connection(region_from, region_to,
                                                       transport_delay_tau - 1);  // -1 for backwards compatibility
                }
            }
        }
    }
}

void ModelInitializer::build_artificial_network() {
    const settings::SettingsNode& network = settings["network"];
    const auto closed = network["closed"].as<bool>(false);
    const auto skewness = network["skewness"].as<unsigned int>();
    if (skewness < 1) {
        throw log::error(this, "Skewness must be >= 1");
    }
    const auto sectors_cnt = network["sectors"].as<unsigned int>();
    for (std::size_t i = 0; i < sectors_cnt; ++i) {
        add_sector("SEC" + std::to_string(i + 1));
    }
    const auto regions_cnt = network["regions"].as<unsigned int>();
    for (std::size_t i = 0; i < regions_cnt; ++i) {
        Region* region = add_region("RG" + std::to_string(i));
        add_consumer("FCON:RG" + std::to_string(i), region);
        for (std::size_t j = 0; j < sectors_cnt; ++j) {
            auto* sector = model()->sectors[j + 1];
            add_firm(sector->name() + ":RG" + std::to_string(i), sector, region);
        }
    }
    build_transport_network();
    const Flow flow = Flow(FlowQuantity(1.0), Price(1.0));
    const Flow double_flow = Flow(FlowQuantity(2.0), Price(1.0));
    for (std::size_t r = 0; r < regions_cnt; ++r) {
        for (std::size_t i = 0; i < sectors_cnt; ++i) {
            auto* firm = model()->sectors[i + 1]->firms[r];
            auto* region = model()->regions[r];
            auto* consumer = model()->economic_agents.find("FCON:" + region->name())->as_consumer();
            log::info(this, firm->name(), "->", model()->sectors[(i + 1) % sectors_cnt + 1]->firms[r]->name(), " = ", flow.get_quantity());
            initialize_connection(firm, model()->sectors[(i + 1) % sectors_cnt + 1]->firms[r], flow);
            if (!closed && r == regions_cnt - 1) {
                log::info(this, firm->name(), "->", consumer->name(), " = ", double_flow.get_quantity());
                initialize_connection(firm, consumer, double_flow);
            } else {
                log::info(this, firm->name(), "->", consumer->name(), " = ", flow.get_quantity());
                initialize_connection(firm, consumer, flow);
            }
            if (closed || r < regions_cnt - 1) {
                log::info(this, firm->name(), "->", model()->sectors[(i + skewness) % sectors_cnt + 1]->firms[(r + 1) % regions_cnt]->name(), " = ",
                          flow.get_quantity());
                initialize_connection(firm, model()->sectors[(i + skewness) % sectors_cnt + 1]->firms[(r + 1) % regions_cnt], flow);
            }
        }
    }
}

void ModelInitializer::build_agent_network() {
    const settings::SettingsNode& network = settings["network"];
    const auto& type = network["type"].as<hashed_string>();
    switch (type) {
            // TODO case hashed_string::hash("csv")

        case hash("netcdf"): {
            const auto& filename = network["file"].as<std::string>();
            netCDF::File file(filename, 'r');

            const auto flow_threshold = network["threshold"].as<FloatType>();
            const Ratio time_factor = model()->delta_t() / Time(365.0);
            const FlowQuantity daily_flow_threshold = round(FlowQuantity(flow_threshold * time_factor));

            const auto sectors_count = file.dimension("sector").require().size();
            for (const auto& sector : file.variable("sector").require().get<std::string>()) {
                add_sector(sector);
            }

            const auto regions_count = file.dimension("region").require().size();
            for (const auto& region : file.variable("region").require().get<std::string>()) {
                add_region(region);
            }

            std::vector<EconomicAgent*> economic_agents;

            auto flows_var = ([&file]() {
                auto tmp = file.variable("flows");
                if (tmp) {
                    return tmp.require();
                }
                return file.variable("flow").require();
            })();

            if (flows_var.check_dimensions({"sector", "region", "sector", "region"})) {
                const auto agents_count = sectors_count * regions_count;
                economic_agents.reserve(agents_count);
                for (auto& sector : model()->sectors) {
                    for (auto& region : model()->regions) {
                        economic_agents.push_back(add_standard_agent(sector.get(), region.get()));
                    }
                }

            } else if (flows_var.check_dimensions({"region", "sector", "region", "sector"})) {
                const auto agents_count = regions_count * sectors_count;
                economic_agents.reserve(agents_count);
                for (auto& region : model()->regions) {
                    for (auto& sector : model()->sectors) {
                        economic_agents.push_back(add_standard_agent(sector.get(), region.get()));
                    }
                }

            } else if (flows_var.check_dimensions({"index"})) {
                const auto agents_count = file.dimension("index").require().size();
                economic_agents.reserve(agents_count);
                const auto index_sector = file.variable("index_sector").require().require_dimensions({"index"}).get<unsigned int>();
                const auto index_region = file.variable("index_region").require().require_dimensions({"index"}).get<unsigned int>();
                for (unsigned int i = 0; i < agents_count; ++i) {
                    economic_agents.push_back(add_standard_agent(model()->sectors[index_sector[i]], model()->regions[index_region[i]]));
                }

            } else if (flows_var.check_dimensions({"flow"})) {
                const auto agents_count = file.dimension("agent").require().size();
                economic_agents.reserve(agents_count);

                int type_firm = -1;
                int type_consumer = -1;

                const auto agent_types = file.variable("agent_type").require().require_dimensions({"agent_type"}).get<std::string>();
                {
                    for (int i = 0; i < agent_types.size(); ++i) {
                        const auto& type = agent_types[i];
                        if (type == "firm") {
                            type_firm = i;
                        } else if (type == "consumer") {
                            type_consumer = i;
                        }
                    }
                }

                struct AgentCompound {
                    char name[25];
                    std::uint8_t agent_type;
                    std::uint32_t sector;
                    std::uint32_t region;
                };
                const auto agents = file.variable("agent").require().require_dimensions({"agent"}).require_compound<AgentCompound>(4).get<AgentCompound>();
                for (unsigned int i = 0; i < agents_count; ++i) {
                    if (agents[i].region >= model()->regions.size()) {
                        throw log::error(this, "Region out of range for ", agents[i].name, " in '", filename, "'");
                    }
                    auto* region = model()->regions[agents[i].region];
                    if (agents[i].agent_type == type_consumer) {
                        economic_agents.push_back(add_consumer(agents[i].name, region));
                    } else if (agents[i].agent_type == type_firm) {
                        if (agents[i].sector >= model()->sectors.size()) {
                            throw log::error(this, "Sector out of range for ", agents[i].name, " in '", filename, "'");
                        }
                        auto* sector = model()->sectors[agents[i].sector];
                        economic_agents.push_back(add_firm(agents[i].name, sector, region));
                    } else {
                        throw log::error(this, "Unkown agent type ", agent_types.at(agents[i].agent_type), " in '", filename, "'");
                    }
                }

            } else {
                throw log::error(this, "Wrong dimensions for flows variable in '", filename, "'");
            }

            build_transport_network();

            const auto agents_count = economic_agents.size();

            if (flows_var.check_dimensions({"flow"})) {
                struct FlowCompound {
                    std::uint32_t agent_from;
                    std::uint32_t agent_to;
                    double value;
                };
                const auto flows = flows_var.require_compound<FlowCompound>(3).get<FlowCompound>();
                for (const auto& flow : flows) {
                    if (flow.value > flow_threshold) {
                        initialize_connection(economic_agents[flow.agent_from]->as_firm(), economic_agents[flow.agent_to],
                                              round(FlowQuantity(flow.value) * time_factor));
                    }
                }

            } else {
                const auto flows = flows_var.require_size(agents_count * agents_count).get<float>();  // use float to save memory
                auto d = std::begin(flows);
                for (auto& source : economic_agents) {
                    if (source->type == EconomicAgent::type_t::FIRM) {
                        Firm* firm_from = source->as_firm();
                        for (auto& target : economic_agents) {
                            const FlowQuantity flow = round(FlowQuantity(*d) * time_factor);
                            if (*d > flow_threshold && flow > daily_flow_threshold) {
                                initialize_connection(firm_from, target, flow);
                            }
                            ++d;
                        }
                    } else {
                        d += agents_count;
                    }
                }
            }

        } break;

        case hash("artificial"): {
            build_artificial_network();
            return;
        } break;

        default:
            throw log::error(this, "Unknown network type '", type, "'");
    }
}

void ModelInitializer::build_transport_network() {  // build transport network before adding the connections
                                                    // to make sure these follow the transport network
    const settings::SettingsNode& transport = settings["transport"];
    const auto& type = transport["type"].as<hashed_string>();
    switch (type) {
        case hash("const"): {
            auto transport_delay = transport["value"].as<TransportDelay>();
            for (auto& region_from : model()->regions) {
                for (auto& region_to : model()->regions) {
                    if (region_to == region_from) {
                        break;
                    }
                    create_simple_transport_connection(region_from.get(), region_to.get(), transport_delay);
                }
            }
        } break;
        case hash("csv"):
            read_transport_times_csv(transport["index"].as<std::string>(), transport["file"].as<std::string>());
            break;
        case hash("centroids"):
            read_centroids_netcdf(transport["file"].as<std::string>());
            break;
        case hash("network"):
            read_transport_network_netcdf(transport["file"].as<std::string>());
            break;
        default:
            throw log::error(this, "Unknown transport type '", type, "'");
    }
}

void ModelInitializer::initialize() {
    pre_initialize();
    build_agent_network();
    clean_network();
    post_initialize();
}

void ModelInitializer::pre_initialize() {
    const settings::SettingsNode& parameters = settings["model"];
    model()->parameters_writable().transport_penalty_small = parameters["transport_penalty_small"].as<Price>();
    model()->parameters_writable().transport_penalty_large = parameters["transport_penalty_large"].as<Price>();
    model()->parameters_writable().optimization_maxiter = parameters["optimization_maxiter"].as<int>();
    model()->parameters_writable().optimization_timeout = parameters["optimization_timeout"].as<unsigned int>();
    model()->parameters_writable().quadratic_transport_penalty = parameters["quadratic_transport_penalty"].as<bool>();
    model()->parameters_writable().maximal_decrease_reservation_price_limited_by_markup =
        parameters["maximal_decrease_reservation_price_limited_by_markup"].as<bool>();
    model()->parameters_writable().always_extend_expected_demand_curve = parameters["always_extend_expected_demand_curve"].as<bool>();
    model()->parameters_writable().naive_expectations = parameters["naive_expectations"].as<bool>();
    model()->parameters_writable().deviation_penalty = parameters["deviation_penalty"].as<bool>(false);
    model()->parameters_writable().min_storage = parameters["min_storage"].as<Ratio>(0.0);
    model()->parameters_writable().cheapest_price_range_preserve_seller_price = parameters["cheapest_price_range_preserve_seller_price"].as<bool>(false);
    model()->parameters_writable().cheapest_price_range_generic_size = (parameters["cheapest_price_range_width"].as<std::string>() == "auto");
    if (!model()->parameters_writable().cheapest_price_range_generic_size) {
        model()->parameters_writable().cheapest_price_range_width = parameters["cheapest_price_range_width"].as<Price>();
    }
    model()->parameters_writable().relative_transport_penalty = parameters["relative_transport_penalty"].as<bool>();
    model()->parameters_writable().optimization_algorithm = optimization::get_algorithm(parameters["optimization_algorithm"].as<hashed_string>("slsqp"));
    if (parameters["cost_correction"].as<bool>(false)) {
        throw log::error(this, "parameter cost_correction not supported anymore");
    }
}

void ModelInitializer::post_initialize() {
    // initialize price dependent members of each capacity manager, which can only be calculated after the whole network has been initialized
    for (auto& sector : model()->sectors) {
        for (auto& firm : sector->firms) {
            firm->sales_manager->initialize();
        }
    }
}

}  // namespace acclimate
