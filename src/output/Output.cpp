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

#include "output/Output.h"

#include <ctime>
#include <istream>
#include <memory>
#include <utility>
#include <vector>

#include "model/BusinessConnection.h"
#include "model/CapacityManager.h"
#include "model/Consumer.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/Government.h"
#include "model/Model.h"
#include "model/PurchasingManager.h"
#include "model/Region.h"
#include "model/SalesManager.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "scenario/RasteredScenario.h"
#include "scenario/Scenario.h"

namespace acclimate {

Output::Output(const settings::SettingsNode& settings, Model* model_p, Scenario* scenario_p, settings::SettingsNode output_node_p)
    : model_m(model_p), scenario(scenario_p), output_node(std::move(output_node_p)) {
    start_time = 0;
    std::ostringstream ss;
    ss << settings;
    settings_string = ss.str();
}

void Output::start() {
    start_time = time(nullptr);
    internal_write_header(localtime(&start_time), model()->run()->thread_count());
    internal_write_settings();
    internal_start();
}

inline void Output::parameter_not_found(const std::string& name) const {
    UNUSED(name);
    if (is_first_timestep()) {
        warning("Parameter '" << name << "' unknown");
    }
}

inline void Output::internal_write_value(const hstring& name, FloatType v) { internal_write_value(name, v, hstring::null()); }

inline void Output::internal_write_value(const hstring& name, const Stock& v) {
    internal_write_value(name, v.get_quantity(), hstring::null());
    internal_write_value(name, v.get_price(), hstring("_price"));
    internal_write_value(name, v.get_value(), hstring("_value"));
}

inline void Output::internal_write_value(const hstring& name, const Flow& v) {
    internal_write_value(name, v.get_quantity(), hstring::null());
    internal_write_value(name, v.get_price(), hstring("_price"));
    internal_write_value(name, v.get_value(), hstring("_value"));
}

template<int precision_digits_p, bool rounded>
inline void Output::internal_write_value(const hstring& name, const Type<precision_digits_p, rounded>& v, const hstring& suffix) {
    internal_write_value(name, to_float(v), suffix);
}

void Output::write_firm_parameters(const Firm* p, const settings::SettingsNode& it) {
    for (const auto& observable : it["parameters"].as_sequence()) {
        const hstring& name = observable.as<hstring>();
        switch (name) {
            case hstring::hash("direct_loss"):
                internal_write_value(name, p->direct_loss());
                break;
            case hstring::hash("total_loss"):
                internal_write_value(name, p->total_loss());
                break;
            case hstring::hash("total_value_loss"):
                internal_write_value(name, p->total_value_loss());
                break;
            case hstring::hash("production"):
                internal_write_value(name, p->production_X());
                break;
            case hstring::hash("forcing"):
                internal_write_value(name, p->forcing());
                break;
            case hstring::hash("incoming_demand"):
                internal_write_value(name, p->sales_manager->sum_demand_requests_D());
                break;
            case hstring::hash("production_capacity"):
                internal_write_value(name, p->capacity_manager->get_production_capacity_p());
                break;
            case hstring::hash("desired_production_capacity"):
                internal_write_value(name, p->capacity_manager->get_desired_production_capacity_p_tilde());
                break;
            case hstring::hash("possible_production_capacity"):
                internal_write_value(name, p->capacity_manager->get_possible_production_capacity_p_hat());
                break;
            default:
                if (!write_firm_parameter(p, name)) {
                    write_economic_agent_parameter(p, name);  // ignore unknown parameters
                }
                break;
        }
    }
    if (it.has("input_storage")) {
        write_input_storages(p, it["input_storage"]);
    }
    if (it.has("outgoing_connection")) {
        write_outgoing_connections(p, it["outgoing_connection"]);
    }
    if (it.has("consumption_connection")) {
        write_consumption_connections(p, it["consumption_connection"]);
    }
}

bool Output::write_firm_parameter(const Firm* p, const hstring& name) {
    switch (name) {
        case hstring::hash("offer_price"):
            internal_write_value(name, p->sales_manager->communicated_parameters().production_X.get_price());
            break;
        case hstring::hash("expected_offer_price"):
            internal_write_value(name, p->sales_manager->communicated_parameters().offer_price_n_bar);
            break;
        case hstring::hash("expected_production"):
            internal_write_value(name, p->sales_manager->communicated_parameters().expected_production_X);
            break;
        case hstring::hash("communicated_possible_production"):
            internal_write_value(name, p->sales_manager->communicated_parameters().possible_production_X_hat);
            break;
        case hstring::hash("unit_production_costs"):
            internal_write_value(name, p->sales_manager->communicated_parameters().possible_production_X_hat.get_price());
            break;
        case hstring::hash("total_production_costs"):
            internal_write_value(name, p->sales_manager->total_production_costs_C());
            break;
        case hstring::hash("total_revenue"):
            internal_write_value(name, p->sales_manager->total_revenue_R());
            break;
        case hstring::hash("tax"):
            internal_write_value(name, p->sales_manager->get_tax());
            break;
        default:
            return false;
    }
    return true;
}

bool Output::write_economic_agent_parameter(const EconomicAgent* p, const hstring& name) {
    switch (name) {
        case hstring::hash("demand"): {
            auto demand = Demand(0.0);
            for (const auto& is : p->input_storages) {
                demand += is->purchasing_manager->demand_D();
            }
            internal_write_value(name, demand);
        } break;
        case hstring::hash("input_flow"): {
            Flow input_flow = Flow(0.0);
            for (const auto& is : p->input_storages) {
                input_flow += is->last_input_flow_I();
            }
            internal_write_value(name, input_flow);
        } break;
        case hstring::hash("used_flow"):
        case hstring::hash("consumption"): {
            Flow used_flow = Flow(0.0);
            for (const auto& is : p->input_storages) {
                used_flow += is->used_flow_U();
            }
            internal_write_value(name, used_flow);
        } break;
        case hstring::hash("business_connections"): {
            int business_connections = 0;
            for (const auto& is : p->input_storages) {
                business_connections += is->purchasing_manager->business_connections.size();
            }
            internal_write_value(name, business_connections);
        } break;
        case hstring::hash("storage"): {
            auto sum_storage_content = Stock(0.0);
            for (const auto& is : p->input_storages) {
                sum_storage_content += is->content_S();
            }
            internal_write_value(name, sum_storage_content);
        } break;
        default:
            return false;
    }
    return true;
}

void Output::write_consumer_parameters(const Consumer* c, const settings::SettingsNode& it) {
    for (const auto& observable : it["parameters"].as_sequence()) {
        const hstring& name = observable.as<hstring>();
        switch (name) {
            case hstring::hash("forcing"):
                internal_write_value(name, c->forcing());
                break;
            default:
                if (!write_consumer_parameter(c, name)) {
                    write_economic_agent_parameter(c, name);  // ignore unknown parameters
                }
                break;
        }
    }
    if (it.has("input_storage")) {
        write_input_storages(c, it["input_storage"]);
    }
}

bool Output::write_consumer_parameter(const Consumer* c, const hstring& name) {
    UNUSED(c);
    UNUSED(name);
    return false;
}

void Output::write_input_storage_parameters(const Storage* s, const settings::SettingsNode& it) {
    for (const auto& observable : it["parameters"].as_sequence()) {
        const hstring& name = observable.as<hstring>();
        switch (name) {
            case hstring::hash("content"):
                internal_write_value(name, s->content_S());
                break;
            case hstring::hash("input_flow"):
                internal_write_value(name, s->last_input_flow_I());
                break;
            case hstring::hash("possible_use"):
                internal_write_value(name, (s->content_S() / model()->delta_t() + s->last_input_flow_I()));
                break;
            case hstring::hash("used_flow"):
                internal_write_value(name, s->used_flow_U());
                break;
            case hstring::hash("desired_used_flow"):
                internal_write_value(name, s->desired_used_flow_U_tilde());
                break;
            case hstring::hash("demand"):
                internal_write_value(name, s->purchasing_manager->demand_D());
                break;
            case hstring::hash("shipment"):
                internal_write_value(name, s->purchasing_manager->get_sum_of_last_shipments());
                break;
            case hstring::hash("business_connections"):
                internal_write_value(name, s->purchasing_manager->business_connections.size());
                break;
            default:
                if (!write_input_storage_parameter(s, name)) {
                    parameter_not_found(name);
                }
                break;
        }
    }
}

bool Output::write_input_storage_parameter(const Storage* s, const hstring& name) {
    switch (name) {
        case hstring::hash("optimized_value"):
            internal_write_value(name, s->purchasing_manager->optimized_value());
            break;
        case hstring::hash("expected_costs"):
            internal_write_value(name, s->purchasing_manager->expected_costs());
            break;
        case hstring::hash("total_transport_penalty"):
            internal_write_value(name, s->purchasing_manager->total_transport_penalty());
            break;
        case hstring::hash("storage_demand"):
            internal_write_value(name, s->purchasing_manager->storage_demand());
            break;
        case hstring::hash("purchase"):
            internal_write_value(name, s->purchasing_manager->purchase());
            break;
        case hstring::hash("use"):
            internal_write_value(name, s->purchasing_manager->demand_D());
            break;
        default:
            return false;
    }
    return true;
}

void Output::write_input_storages(const EconomicAgent* ea, const settings::SettingsNode& it) {
    for (const auto& input_storage_node : it.as_sequence()) {
        const char* storage_sector_name = nullptr;
        if (input_storage_node.has("sector")) {
            storage_sector_name = input_storage_node["sector"].as<std::string>().c_str();
        }
        for (const auto& is : ea->input_storages) {
            if ((storage_sector_name == nullptr) || is->sector->id() == storage_sector_name) {
                if (ea->type == EconomicAgent::Type::CONSUMER) {
                    internal_start_target(hstring("consumers/input_storages"), is->sector);
                } else {
                    internal_start_target(hstring("firms/input_storages"), is->sector);
                    if (input_storage_node.has("ingoing_connection")) {
                        write_ingoing_connections(is.get(), input_storage_node["ingoing_connection"]);
                    }
                }
                write_input_storage_parameters(is.get(), input_storage_node["parameters"]);
                internal_end_target();
                if (storage_sector_name != nullptr) {
                    break;
                }
            }
        }
    }
}

void Output::write_connection_parameters(const BusinessConnection* b, const settings::SettingsNode& it) {
    for (const auto& observable : it["parameters"].as_sequence()) {
        const hstring& name = observable.as<hstring>();
        switch (name) {
            case hstring::hash("sent_flow"):
                internal_write_value(name, b->last_shipment_Z());
                break;
            case hstring::hash("received_flow"):
                internal_write_value(name, b->last_delivery_Z());
                break;
            case hstring::hash("demand_request"):
                internal_write_value(name, b->last_demand_request_D());
                break;
            case hstring::hash("total_flow"):
                internal_write_value(name, b->get_total_flow());
                break;
            case hstring::hash("flow_mean"):
                internal_write_value(name, b->get_flow_mean());
                break;
            case hstring::hash("initial_flow"):
                if (is_first_timestep()) {
                    internal_write_value(name, b->initial_flow_Z_star());
                }
                break;
            case hstring::hash("flow_deficit"):
                internal_write_value(name, b->get_flow_deficit());
                break;
            default:
                if (!write_connection_parameter(b, name)) {
                    parameter_not_found(name);
                }
                break;
        }
    }
}

bool Output::write_connection_parameter(const BusinessConnection* b, const hstring& name) {
    UNUSED(b);
    UNUSED(name);
    return false;
}

void Output::write_outgoing_connections(const Firm* p, const settings::SettingsNode& it) {
    for (const auto& outgoing_connection_node : it.as_sequence()) {
        const char* sector_to_name = nullptr;
        const char* region_to_name = nullptr;
        if (outgoing_connection_node.has("sector")) {
            sector_to_name = outgoing_connection_node["sector"].as<std::string>().c_str();
        }
        if (!outgoing_connection_node.has("region")) {
            region_to_name = outgoing_connection_node["region"].as<std::string>().c_str();
        }
        for (const auto& bc : p->sales_manager->business_connections) {
            if (bc->buyer->storage->economic_agent->type == EconomicAgent::Type::FIRM) {
                Firm* firm_to = bc->buyer->storage->economic_agent->as_firm();
                if ((sector_to_name == nullptr) || firm_to->sector->id() == sector_to_name) {
                    if ((region_to_name == nullptr) || firm_to->region->id() == region_to_name) {
                        internal_start_target(hstring("outgoing_connections"), firm_to->sector, firm_to->region);
                        write_connection_parameters(bc.get(), outgoing_connection_node["parameters"]);
                        internal_end_target();
                    }
                }
            }
        }
    }
}

void Output::write_ingoing_connections(const Storage* s, const settings::SettingsNode& it) {
    for (const auto& ingoing_connection_node : it.as_sequence()) {
        const char* sector_from_name = nullptr;
        const char* region_from_name = nullptr;
        if (ingoing_connection_node.has("sector")) {
            sector_from_name = ingoing_connection_node["sector"].as<std::string>().c_str();
        }
        if (ingoing_connection_node.has("region")) {
            region_from_name = ingoing_connection_node["region"].as<std::string>().c_str();
        }
        for (const auto& bc : s->purchasing_manager->business_connections) {
            Firm* firm_from = bc->seller->firm;
            if ((sector_from_name == nullptr) || firm_from->sector->id() == sector_from_name) {
                if ((region_from_name == nullptr) || firm_from->region->id() == region_from_name) {
                    internal_start_target(hstring("ingoing_connections"), firm_from->sector, firm_from->region);
                    write_connection_parameters(bc.get(), ingoing_connection_node["parameters"]);
                    internal_end_target();
                }
            }
        }
    }
}

void Output::write_consumption_connections(const Firm* p, const settings::SettingsNode& it) {
    for (const auto& outgoing_connection_node : it.as_sequence()) {
        const char* region_to_name = nullptr;
        if (outgoing_connection_node.has("region")) {
            region_to_name = outgoing_connection_node["region"].as<std::string>().c_str();
        }
        for (const auto& bc : p->sales_manager->business_connections) {
            if (bc->buyer->storage->economic_agent->type == EconomicAgent::Type::CONSUMER) {
                Consumer* consumer_to = bc->buyer->storage->economic_agent->as_consumer();
                if ((region_to_name == nullptr) || consumer_to->region->id() == region_to_name) {
                    internal_start_target(hstring("consumption_connections"), consumer_to->region);
                    write_connection_parameters(bc.get(), outgoing_connection_node["parameters"]);
                    internal_end_target();
                    if (region_to_name != nullptr) {
                        break;
                    }
                }
            }
        }
    }
}

void Output::write_region_parameters(const Region* region, const settings::SettingsNode& it) {
    for (const auto& observable : it["parameters"].as_sequence()) {
        const hstring& name = observable.as<hstring>();
        switch (name) {
            case hstring::hash("import"):
                internal_write_value(name, region->import_flow_Z());
                break;
            case hstring::hash("export"):
                internal_write_value(name, region->export_flow_Z());
                break;
            case hstring::hash("consumption"):
                internal_write_value(name, region->consumption_C());
                break;
            case hstring::hash("gdp"):
                internal_write_value(name, region->get_gdp());
                break;
            case hstring::hash("government_budget"):
                if (region->government() != nullptr) {
                    internal_write_value(name, region->government()->budget());
                }
                break;
            case hstring::hash("total_flow"): {
                std::vector<Flow> flows(model()->regions.size(), Flow(0.0));
                for (const auto& ea : region->economic_agents) {
                    if (ea->is_firm()) {
                        for (const auto& bc : ea->as_firm()->sales_manager->business_connections) {
                            flows[bc->buyer->storage->economic_agent->region->index()] += bc->get_total_flow();
                        }
                    }
                }
                for (const auto& region_to : model()->regions) {
                    internal_start_target(hstring("regions"), region_to.get());
                    internal_write_value(hstring("total_flow"), flows[region_to->index()]);
                    internal_end_target();
                }
            } break;
            case hstring::hash("sent_flow"): {
                std::vector<Flow> flows(model()->regions.size(), Flow(0.0));
                for (const auto& ea : region->economic_agents) {
                    if (ea->is_firm()) {
                        for (const auto& bc : ea->as_firm()->sales_manager->business_connections) {
                            flows[bc->buyer->storage->economic_agent->region->index()] += bc->last_shipment_Z();
                        }
                    }
                }
                for (const auto& region_to : model()->regions) {
                    internal_start_target(hstring("regions"), region_to.get());
                    internal_write_value(hstring("sent_flow"), flows[region_to->index()]);
                    internal_end_target();
                }
            } break;
            case hstring::hash("received_flows"): {
                std::vector<Flow> flows(model()->regions.size(), Flow(0.0));
                for (const auto& ea : region->economic_agents) {
                    if (ea->is_firm()) {
                        for (const auto& bc : ea->as_firm()->sales_manager->business_connections) {
                            flows[bc->buyer->storage->economic_agent->region->index()] += bc->last_delivery_Z();
                        }
                    }
                }
                for (const auto& region_to : model()->regions) {
                    internal_start_target(hstring("regions"), region_to.get());
                    internal_write_value(hstring("received_flow"), flows[region_to->index()]);
                    internal_end_target();
                }
            } break;
            default:
                if (!write_region_parameter(region, name)) {
                    parameter_not_found(name);
                }
                break;
        }
    }
}

bool Output::write_region_parameter(const Region* region, const hstring& name) {
    UNUSED(region);
    UNUSED(name);
    return false;
}

void Output::write_sector_parameters(const Sector* sector, const settings::SettingsNode& parameters) {
    for (const auto& observable : parameters.as_sequence()) {
        const hstring& name = observable.as<hstring>();
        switch (name) {
            case hstring::hash("total_production"):
                internal_write_value(name, sector->total_production_X());
                break;
            default:
                if (!write_sector_parameter(sector, name)) {
                    parameter_not_found(name);
                }
                break;
        }
    }
}

bool Output::write_sector_parameter(const Sector* sector, const hstring& name) {
    switch (name) {
        case hstring::hash("total_demand"):
            internal_write_value(name, sector->total_demand_D());
            break;
        case hstring::hash("offer_price"):
            internal_write_value(name, sector->total_production_X().get_price());
            break;
        default:
            return false;
    }
    return true;
}

void Output::iterate() {
    internal_iterate_begin();
    if (output_node.has("observables")) {
        for (const auto& observables : output_node["observables"].as_sequence()) {
            for (const auto& observable : observables.as_map()) {
                const auto name = hstring(observable.first);
                const settings::SettingsNode& it = observable.second;
                switch (name) {
                    case hstring::hash("firm"): {
                        if (!it.has("set")) {
                            if (!it.has("sector")) {
                                if (!it.has("region")) {
                                    for (const auto& sector : model()->sectors) {
                                        for (const auto& p : sector->firms) {
                                            internal_start_target(hstring("firms"), p->sector, p->region);
                                            write_firm_parameters(p, it);
                                            internal_end_target();
                                        }
                                    }
                                } else {
                                    const Region* region = model()->find_region(it["region"].as<std::string>());
                                    if (region != nullptr) {
                                        for (const auto& ea : region->economic_agents) {
                                            if (ea->type == EconomicAgent::Type::FIRM) {
                                                const Firm* p = ea->as_firm();
                                                internal_start_target(hstring("firms"), p->sector, p->region);
                                                write_firm_parameters(p, it);
                                                internal_end_target();
                                            }
                                        }
                                    } else {
                                        warning("Region " << it["region"].as<std::string>() << " not found");
                                    }
                                }
                            } else {
                                if (!it.has("region")) {
                                    const Sector* sector = model()->find_sector(it["sector"].as<std::string>());
                                    if (sector != nullptr) {
                                        for (const auto& p : sector->firms) {
                                            internal_start_target(hstring("firms"), p->sector, p->region);
                                            write_firm_parameters(p, it);
                                            internal_end_target();
                                        }
                                    } else {
                                        warning("Sector " << it["sector"].as<std::string>() << " not found");
                                    }
                                } else {
                                    const Firm* p = model()->find_firm(it["sector"].as<std::string>(), it["region"].as<std::string>());
                                    if (p != nullptr) {
                                        internal_start_target(hstring("firms"), p->sector, p->region);
                                        write_firm_parameters(p, it);
                                        internal_end_target();
                                    } else {
                                        warning("Firm " << it["sector"].as<std::string>() << ":" << it["region"].as<std::string>() << " not found");
                                    }
                                }
                            }
                        } else {
                            std::istringstream ss(it["set"].as<std::string>());
                            std::string ps;
                            while (getline(ss, ps, ',')) {
                                std::istringstream ss_l(ps);
                                std::string sector_name;
                                std::string region_name;
                                getline(ss_l, sector_name, ':');
                                getline(ss_l, region_name, ':');
                                const Firm* p = model()->find_firm(sector_name, region_name);
                                if (p != nullptr) {
                                    internal_start_target(hstring("firms"), p->sector, p->region);
                                    write_firm_parameters(p, it);
                                    internal_end_target();
                                } else {
                                    warning("Firm " << it["sector"].as<std::string>() << ":" << it["region"].as<std::string>() << " not found");
                                }
                            }
                        }
                    } break;

                    case hstring::hash("consumer"): {
                        if (!it.has("region")) {
                            for (const auto& region : model()->regions) {
                                for (const auto& ea : region->economic_agents) {
                                    if (ea->type == EconomicAgent::Type::CONSUMER) {
                                        Consumer* c = ea->as_consumer();
                                        internal_start_target(hstring("consumers"), c->region);
                                        write_consumer_parameters(c, it);
                                        internal_end_target();
                                    }
                                }
                            }
                        } else {
                            Consumer* c = model()->find_consumer(it["region"].as<std::string>());
                            if (c != nullptr) {
                                internal_start_target(hstring("consumers"), c->region);
                                write_consumer_parameters(c, it);
                                internal_end_target();
                            } else {
                                warning("Consumer " << it["region"].as<std::string>() << " not found");
                            }
                        }
                    } break;

                    case hstring::hash("agent"): {
                        for (const auto& region : model()->regions) {
                            for (const auto& ea : region->economic_agents) {
                                internal_start_target(hstring("agents"), ea->sector, ea->region);
                                if (ea->type == EconomicAgent::Type::CONSUMER) {
                                    write_consumer_parameters(ea->as_consumer(), it);
                                } else {
                                    write_firm_parameters(ea->as_firm(), it);
                                }
                                internal_end_target();
                            }
                        }
                    } break;

                    case hstring::hash("storage"): {
                        for (const auto& region : model()->regions) {
                            for (const auto& ea : region->economic_agents) {
                                internal_start_target(hstring("storages"), ea->sector, ea->region);
                                for (const auto& is : ea->input_storages) {
                                    internal_start_target(hstring("storages"), is->sector);
                                    write_input_storage_parameters(is.get(), it);
                                    internal_end_target();
                                }
                                internal_end_target();
                            }
                        }
                    } break;

                    case hstring::hash("flow"): {
                        for (const auto& sector : model()->sectors) {
                            for (const auto& ps : sector->firms) {
                                internal_start_target(hstring("flows"), ps->sector, ps->region);
                                for (const auto& bc : ps->sales_manager->business_connections) {
                                    internal_start_target(hstring("flows"), bc->buyer->storage->economic_agent->sector,
                                                          bc->buyer->storage->economic_agent->region);
                                    write_connection_parameters(bc.get(), it);
                                    internal_end_target();
                                }
                                internal_end_target();
                            }
                        }
                    } break;

                    case hstring::hash("region"): {
                        const char* region_name = nullptr;
                        if (it.has("name")) {
                            region_name = it["name"].as<std::string>().c_str();
                        }
                        if (static_cast<RasteredScenario<FloatType>*>(scenario) != nullptr) {
                            for (const auto& observable : it["parameters"].as_sequence()) {
                                const hstring& name = observable.as<hstring>();
                                switch (name) {
                                    case hstring::hash("total_current_proxy_sum"):
                                        for (const auto& forcing :
                                             static_cast<RasteredScenario<FloatType>*>(scenario)  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
                                                 ->forcings()) {
                                            if (forcing.region != nullptr && ((region_name == nullptr) || forcing.region->id() == region_name)) {
                                                internal_start_target(hstring("regions"), forcing.region);
                                                internal_write_value(name, forcing.forcing);
                                                internal_end_target();
                                            }
                                        }
                                        break;
                                }
                            }
                        }
                        for (const auto& region : model()->regions) {
                            if ((region_name == nullptr) || region->id() == region_name) {
                                internal_start_target(hstring("regions"), region.get());
                                write_region_parameters(region.get(), it);
                                internal_end_target();
                            }
                        }
                    } break;

                    case hstring::hash("sector"): {
                        const char* sector_name = nullptr;
                        if (it.has("sector")) {
                            sector_name = it["sector"].as<std::string>().c_str();
                        }
                        for (const auto& sector : model()->sectors) {
                            if (sector->id() == "FCON") {
                                continue;
                            }
                            if ((sector_name == nullptr) || sector->id() == sector_name) {
                                internal_start_target(hstring("sectors"), sector.get());
                                write_sector_parameters(sector.get(), it);
                                internal_end_target();
                            }
                        }
                    } break;

                    case hstring::hash("meta"): {
                        internal_start_target(hstring("meta"));
                        for (const auto& observable : it["parameters"].as_sequence()) {
                            const hstring& name = observable.as<hstring>();
                            switch (name) {
                                case hstring::hash("duration"):
                                    internal_write_value(name, model()->run()->duration());
                                    break;
                                default:
                                    parameter_not_found(name);
                                    break;
                            }
                        }
                        internal_end_target();
                    } break;

                    default:
                        parameter_not_found(name);
                        break;
                }
            }
        }
    }
    internal_iterate_end();
}

void Output::end() {
    time_t end_time = time(nullptr);
    time_t duration = end_time - start_time;
    internal_write_footer(localtime(&duration));
    internal_end();
}

void Output::internal_write_value(const hstring& name, FloatType v, const hstring& suffix) {
    UNUSED(name);
    UNUSED(suffix);
    UNUSED(v);
}

void Output::internal_write_header(tm* timestamp, unsigned int max_threads) {
    UNUSED(timestamp);
    UNUSED(max_threads);
}

void Output::internal_write_footer(tm* duration) { UNUSED(duration); }

void Output::internal_write_settings() {}

void Output::internal_start() {}

void Output::internal_iterate_begin() {}

void Output::internal_iterate_end() {}

void Output::internal_end() {}

void Output::internal_start_target(const hstring& name, Sector* sector, Region* region) {
    UNUSED(name);
    UNUSED(sector);
    UNUSED(region);
}

void Output::internal_start_target(const hstring& name, Sector* sector) {
    UNUSED(name);
    UNUSED(sector);
}

void Output::internal_start_target(const hstring& name, Region* region) {
    UNUSED(region);
    UNUSED(name);
}

void Output::internal_start_target(const hstring& name) { UNUSED(name); }

void Output::internal_end_target() {}

void Output::event(EventType type, const Sector* sector_from, const Region* region_from, const Sector* sector_to, const Region* region_to, FloatType value) {
    UNUSED(type);
    UNUSED(sector_from);
    UNUSED(region_from);
    UNUSED(sector_to);
    UNUSED(region_to);
    UNUSED(value);
}

void Output::event(EventType type, const Sector* sector_from, const Region* region_from, const EconomicAgent* economic_agent_to, FloatType value) {
    event(type, sector_from, region_from, economic_agent_to == nullptr ? nullptr : economic_agent_to->sector,
          economic_agent_to == nullptr ? nullptr : economic_agent_to->region, value);
}

void Output::event(EventType type, const EconomicAgent* economic_agent_from, const EconomicAgent* economic_agent_to, FloatType value) {
    event(type, economic_agent_from == nullptr ? nullptr : economic_agent_from->sector, economic_agent_from == nullptr ? nullptr : economic_agent_from->region,
          economic_agent_to == nullptr ? nullptr : economic_agent_to->as_firm()->sector, economic_agent_to == nullptr ? nullptr : economic_agent_to->region,
          value);
}

void Output::event(EventType type, const EconomicAgent* economic_agent_from, const Sector* sector_to, const Region* region_to, FloatType value) {
    event(type, economic_agent_from == nullptr ? nullptr : economic_agent_from->sector, economic_agent_from == nullptr ? nullptr : economic_agent_from->region,
          sector_to, region_to, value);
}

bool Output::is_first_timestep() const { return scenario->is_first_timestep(); }

}  // namespace acclimate
