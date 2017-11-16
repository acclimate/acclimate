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
#include "model/BusinessConnection.h"
#include "model/Consumer.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/PurchasingManagerPrices.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "scenario/RasteredScenario.h"
#include "scenario/Scenario.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Output<ModelVariant>::Output(const settings::SettingsNode& settings_p,
                             Model<ModelVariant>* model_p,
                             Scenario<ModelVariant>* scenario_p,
                             const settings::SettingsNode& output_node_p)
    : settings(settings_p), model(model_p), scenario(scenario_p), output_node(output_node_p) {
    start_time = 0;
}

template<class ModelVariant>
void Output<ModelVariant>::start() {
    start_time = time(nullptr);
    internal_write_header(localtime(&start_time), Acclimate::instance()->thread_count());
    internal_write_settings();
    internal_start();
}

template<class ModelVariant>
inline void Output<ModelVariant>::parameter_not_found(const std::string& name) const {
    UNUSED(name);
    if (is_first_timestep()) {
        warning("Parameter '" << name << "' unknown");
    }
}

template<class ModelVariant>
void Output<ModelVariant>::internal_write_value(const std::string& name, const Stock& v) {
    internal_write_value(name, v.get_quantity());
    internal_write_value(name + "_price", v.get_price());
    internal_write_value(name + "_value", v.get_value());
}

template<class ModelVariant>
void Output<ModelVariant>::internal_write_value(const std::string& name, const Flow& v) {
    internal_write_value(name, v.get_quantity());
    internal_write_value(name + "_price", v.get_price());
    internal_write_value(name + "_value", v.get_value());
}

template<class ModelVariant>
template<int precision_digits_p>
inline void Output<ModelVariant>::internal_write_value(const std::string& name, const Type<precision_digits_p>& v) {
    internal_write_value(name, to_float(v));
}

template<class ModelVariant>
void Output<ModelVariant>::write_firm_parameters(const Firm<ModelVariant>* p, const settings::SettingsNode& it) {
    for (const auto& observable : it["parameters"].as_sequence()) {
        const settings::hstring& name = observable.as<settings::hstring>();
        switch (name) {
            case settings::hstring::hash("direct_loss"):
                internal_write_value(name, p->direct_loss());
                break;
            case settings::hstring::hash("total_loss"):
                internal_write_value(name, p->total_loss());
                break;
            case settings::hstring::hash("total_value_loss"):
                internal_write_value(name, p->total_value_loss());
                break;
            case settings::hstring::hash("production"):
                internal_write_value(name, p->production_X());
                break;
            case settings::hstring::hash("forcing"):
                internal_write_value(name, p->forcing());
                break;
            case settings::hstring::hash("incoming_demand"):
                internal_write_value(name, p->sales_manager->sum_demand_requests_D());
                break;
            case settings::hstring::hash("production_capacity"):
                internal_write_value(name, p->capacity_manager->get_production_capacity_p());
                break;
            case settings::hstring::hash("desired_production_capacity"):
                internal_write_value(name, p->capacity_manager->get_desired_production_capacity_p_tilde());
                break;
            case settings::hstring::hash("possible_production_capacity"):
                internal_write_value(name, p->capacity_manager->get_possible_production_capacity_p_hat());
                break;
            default:
                if (!write_firm_parameter_variant(p, name)) {
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

template<class ModelVariant>
bool Output<ModelVariant>::write_firm_parameter_variant(const Firm<ModelVariant>* p, const settings::hstring& name) {
    UNUSED(p);
    UNUSED(name);
    return false;
}

template<>
bool Output<VariantPrices>::write_firm_parameter_variant(const Firm<VariantPrices>* p, const settings::hstring& name) {
    switch (name) {
        case settings::hstring::hash("offer_price"):
            internal_write_value(name, p->sales_manager->communicated_parameters().production_X.get_price());
            break;
        case settings::hstring::hash("expected_offer_price"):
            internal_write_value(name, p->sales_manager->communicated_parameters().offer_price_n_bar);
            break;
        case settings::hstring::hash("expected_production"):
            internal_write_value(name, p->sales_manager->communicated_parameters().expected_production_X);
            break;
        case settings::hstring::hash("communicated_possible_production"):
            internal_write_value(name, p->sales_manager->communicated_parameters().possible_production_X_hat);
            break;
        case settings::hstring::hash("unit_production_costs"):
            internal_write_value(name, p->sales_manager->communicated_parameters().possible_production_X_hat.get_price());
            break;
        case settings::hstring::hash("total_production_costs"):
            internal_write_value(name, p->sales_manager->total_production_costs_C());
            break;
        case settings::hstring::hash("total_revenue"):
            internal_write_value(name, p->sales_manager->total_revenue_R());
            break;
        case settings::hstring::hash("tax"):
            internal_write_value(name, p->sales_manager->get_tax());
            break;
        default:
            return false;
    }
    return true;
}

template<class ModelVariant>
bool Output<ModelVariant>::write_economic_agent_parameter(const EconomicAgent<ModelVariant>* p, const settings::hstring& name) {
    switch (name) {
        case settings::hstring::hash("demand"): {
            Demand demand = Demand(0.0);
            for (const auto& is : p->input_storages) {
                demand += is->purchasing_manager->demand_D();
            }
            internal_write_value(name, demand);
        } break;
        case settings::hstring::hash("input_flow"): {
            Flow input_flow = Flow(0.0);
            for (const auto& is : p->input_storages) {
                input_flow += is->last_input_flow_I();
            }
            internal_write_value(name, input_flow);
        } break;
        case settings::hstring::hash("used_flow"):
        case settings::hstring::hash("consumption"): {
            Flow used_flow = Flow(0.0);
            for (const auto& is : p->input_storages) {
                used_flow += is->used_flow_U();
            }
            internal_write_value(name, used_flow);
        } break;
        case settings::hstring::hash("business_connections"): {
            int business_connections = 0;
            for (const auto& is : p->input_storages) {
                business_connections += is->purchasing_manager->business_connections.size();
            }
            internal_write_value(name, business_connections);
        } break;
        case settings::hstring::hash("storage"): {
            Stock sum_storage_content = Stock(0.0);
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

template<class ModelVariant>
void Output<ModelVariant>::write_consumer_parameters(const Consumer<ModelVariant>* c, const settings::SettingsNode& it) {
    for (const auto& observable : it["parameters"].as_sequence()) {
        const settings::hstring& name = observable.as<settings::hstring>();
        switch (name) {
            case settings::hstring::hash("forcing"):
                internal_write_value(name, c->forcing());
                break;
            default:
                if (!write_consumer_parameter_variant(c, name)) {
                    write_economic_agent_parameter(c, name);  // ignore unknown parameters
                }
                break;
        }
    }
    if (it.has("input_storage")) {
        write_input_storages(c, it["input_storage"]);
    }
}

template<class ModelVariant>
bool Output<ModelVariant>::write_consumer_parameter_variant(const Consumer<ModelVariant>* c, const settings::hstring& name) {
    UNUSED(c);
    UNUSED(name);
    return false;
}

template<class ModelVariant>
void Output<ModelVariant>::write_input_storage_parameters(const Storage<ModelVariant>* s, const settings::SettingsNode& it) {
    for (const auto& observable : it["parameters"].as_sequence()) {
        const settings::hstring& name = observable.as<settings::hstring>();
        switch (name) {
            case settings::hstring::hash("content"):
                internal_write_value(name, s->content_S());
                break;
            case settings::hstring::hash("input_flow"):
                internal_write_value(name, s->last_input_flow_I());
                break;
            case settings::hstring::hash("possible_use"):
                internal_write_value(name, (s->content_S() / s->sector->model->delta_t() + s->last_input_flow_I()));
                break;
            case settings::hstring::hash("used_flow"):
                internal_write_value(name, s->used_flow_U());
                break;
            case settings::hstring::hash("desired_used_flow"):
                internal_write_value(name, s->desired_used_flow_U_tilde());
                break;
            case settings::hstring::hash("demand"):
                internal_write_value(name, s->purchasing_manager->demand_D());
                break;
            case settings::hstring::hash("shipment"):
                internal_write_value(name, s->purchasing_manager->get_sum_of_last_shipments());
                break;
            case settings::hstring::hash("business_connections"):
                internal_write_value(name, s->purchasing_manager->business_connections.size());
                break;
            default:
                if (!write_input_storage_parameter_variant(s, name)) {
                    parameter_not_found(name);
                }
                break;
        }
    }
}

template<class ModelVariant>
bool Output<ModelVariant>::write_input_storage_parameter_variant(const Storage<ModelVariant>* s, const settings::hstring& name) {
    UNUSED(s);
    UNUSED(name);
    return false;
}

template<>
bool Output<VariantPrices>::write_input_storage_parameter_variant(const Storage<VariantPrices>* s, const settings::hstring& name) {
    switch (name) {
        case settings::hstring::hash("optimized_value"):
            internal_write_value(name, s->purchasing_manager->optimized_value());
            break;
        case settings::hstring::hash("expected_costs"):
            internal_write_value(name, s->purchasing_manager->expected_costs());
            break;
        case settings::hstring::hash("total_transport_penalty"):
            internal_write_value(name, s->purchasing_manager->total_transport_penalty());
            break;
        case settings::hstring::hash("storage_demand"):
            internal_write_value(name, s->purchasing_manager->storage_demand());
            break;
        case settings::hstring::hash("purchase"):
            internal_write_value(name, s->purchasing_manager->purchase());
            break;
        case settings::hstring::hash("use"):
            internal_write_value(name, s->purchasing_manager->demand_D());
            break;
        default:
            return false;
    }
    return true;
}

template<class ModelVariant>
void Output<ModelVariant>::write_input_storages(const EconomicAgent<ModelVariant>* ea, const settings::SettingsNode& it) {
    for (const auto& input_storage_node : it.as_sequence()) {
        const char* storage_sector_name = nullptr;
        if (input_storage_node.has("sector")) {
            storage_sector_name = input_storage_node["sector"].as<std::string>().c_str();
        }
        for (const auto& is : ea->input_storages) {
            if ((storage_sector_name == nullptr) || std::string(*is->sector) == storage_sector_name) {
                if (ea->type == EconomicAgent<ModelVariant>::Type::CONSUMER) {
                    internal_start_target("consumers/input_storages", is->sector);
                } else {
                    internal_start_target("firms/input_storages", is->sector);
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

template<class ModelVariant>
void Output<ModelVariant>::write_connection_parameters(const BusinessConnection<ModelVariant>* b, const settings::SettingsNode& it) {
    for (const auto& observable : it["parameters"].as_sequence()) {
        const settings::hstring& name = observable.as<settings::hstring>();
        switch (name) {
            case settings::hstring::hash("sent_flow"):
                internal_write_value(name, b->last_shipment_Z());
                break;
            case settings::hstring::hash("received_flow"):
                internal_write_value(name, b->last_delivery_Z());
                break;
            case settings::hstring::hash("demand_request"):
                internal_write_value(name, b->last_demand_request_D());
                break;
            case settings::hstring::hash("total_flow"):
                internal_write_value(name, b->get_total_flow());
                break;
            case settings::hstring::hash("flow_mean"):
                internal_write_value(name, b->get_flow_mean());
                break;
            case settings::hstring::hash("initial_flow"):
                if (is_first_timestep()) {
                    internal_write_value(name, b->initial_flow_Z_star());
                }
                break;
            case settings::hstring::hash("flow_deficit"):
                internal_write_value(name, b->get_flow_deficit());
                break;
            default:
                if (!write_connection_parameter_variant(b, name)) {
                    parameter_not_found(name);
                }
                break;
        }
    }
}

template<class ModelVariant>
bool Output<ModelVariant>::write_connection_parameter_variant(const BusinessConnection<ModelVariant>* b, const settings::hstring& name) {
    UNUSED(b);
    UNUSED(name);
    return false;
}

template<class ModelVariant>
void Output<ModelVariant>::write_outgoing_connections(const Firm<ModelVariant>* p, const settings::SettingsNode& it) {
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
            if (bc->buyer->storage->economic_agent->type == EconomicAgent<ModelVariant>::Type::FIRM) {
                Firm<ModelVariant>* firm_to = bc->buyer->storage->economic_agent->as_firm();
                if ((sector_to_name == nullptr) || std::string(*firm_to->sector) == sector_to_name) {
                    if ((region_to_name == nullptr) || std::string(*firm_to->region) == region_to_name) {
                        internal_start_target("outgoing_connections", firm_to->sector, firm_to->region);
                        write_connection_parameters(bc.get(), outgoing_connection_node["parameters"]);
                        internal_end_target();
                    }
                }
            }
        }
    }
}

template<class ModelVariant>
void Output<ModelVariant>::write_ingoing_connections(const Storage<ModelVariant>* s, const settings::SettingsNode& it) {
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
            Firm<ModelVariant>* firm_from = bc->seller->firm;
            if ((sector_from_name == nullptr) || std::string(*firm_from->sector) == sector_from_name) {
                if ((region_from_name == nullptr) || std::string(*firm_from->region) == region_from_name) {
                    internal_start_target("ingoing_connections", firm_from->sector, firm_from->region);
                    write_connection_parameters(bc, ingoing_connection_node["parameters"]);
                    internal_end_target();
                }
            }
        }
    }
}

template<class ModelVariant>
void Output<ModelVariant>::write_consumption_connections(const Firm<ModelVariant>* p, const settings::SettingsNode& it) {
    for (const auto& outgoing_connection_node : it.as_sequence()) {
        const char* region_to_name = nullptr;
        if (outgoing_connection_node.has("region")) {
            region_to_name = outgoing_connection_node["region"].as<std::string>().c_str();
        }
        for (const auto& bc : p->sales_manager->business_connections) {
            if (bc->buyer->storage->economic_agent->type == EconomicAgent<ModelVariant>::Type::CONSUMER) {
                Consumer<ModelVariant>* consumer_to = bc->buyer->storage->economic_agent->as_consumer();
                if ((region_to_name == nullptr) || std::string(*consumer_to->region) == region_to_name) {
                    internal_start_target("consumption_connections", consumer_to->region);
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

template<class ModelVariant>
void Output<ModelVariant>::write_region_parameters(const Region<ModelVariant>* region, const settings::SettingsNode& it) {
    for (const auto& observable : it["parameters"].as_sequence()) {
        const settings::hstring& name = observable.as<settings::hstring>();
        switch (name) {
            case settings::hstring::hash("import"):
                internal_write_value(name, region->import_flow_Z());
                break;
            case settings::hstring::hash("export"):
                internal_write_value(name, region->export_flow_Z());
                break;
            case settings::hstring::hash("consumption"):
                internal_write_value(name, region->consumption_C());
                break;
            case settings::hstring::hash("gdp"):
                internal_write_value(name, region->get_gdp());
                break;
            case settings::hstring::hash("government_budget"):
                if (region->government()) {
                    internal_write_value(name, region->government()->budget());
                }
                break;
            case settings::hstring::hash("people_affected"):
                // handled in iterate
                break;
            default:
                if (!write_region_parameter_variant(region, name)) {
                    parameter_not_found(name);
                }
                break;
        }
    }
}

template<class ModelVariant>
bool Output<ModelVariant>::write_region_parameter_variant(const Region<ModelVariant>* region, const settings::hstring& name) {
    UNUSED(region);
    UNUSED(name);
    return false;
}

template<class ModelVariant>
void Output<ModelVariant>::write_sector_parameters(const Sector<ModelVariant>* sector, const settings::SettingsNode& parameters) {
    for (const auto& observable : parameters.as_sequence()) {
        const settings::hstring& name = observable.as<settings::hstring>();
        switch (name) {
            case settings::hstring::hash("total_production"):
                internal_write_value(name, sector->total_production_X());
                break;
            default:
                if (!write_sector_parameter_variant(sector, name)) {
                    parameter_not_found(name);
                }
                break;
        }
    }
}

template<class ModelVariant>
bool Output<ModelVariant>::write_sector_parameter_variant(const Sector<ModelVariant>* sector, const settings::hstring& name) {
    UNUSED(sector);
    UNUSED(name);
    return false;
}

template<>
bool Output<VariantPrices>::write_sector_parameter_variant(const Sector<VariantPrices>* sector, const settings::hstring& name) {
    switch (name) {
        case settings::hstring::hash("total_demand"):
            internal_write_value(name, sector->total_demand_D());
            break;
        case settings::hstring::hash("offer_price"):
            internal_write_value(name, sector->total_production_X().get_price());
            break;
        default:
            return false;
    }
    return true;
}

template<class ModelVariant>
void Output<ModelVariant>::iterate() {
    internal_iterate_begin();
    for (const auto& observables : output_node["observables"].as_sequence()) {
        for (const auto& observable : observables.as_map()) {
            const auto name = settings::hstring(observable.first);
            const settings::SettingsNode& it = observable.second;
            switch (name) {
                case settings::hstring::hash("firm"): {
                    if (!it.has("set")) {
                        if (!it.has("sector")) {
                            if (!it.has("region")) {
                                for (const auto& sector : model->sectors_C) {
                                    for (const auto& p : sector->firms_N) {
                                        internal_start_target("firms", p->sector, p->region);
                                        write_firm_parameters(p, it);
                                        internal_end_target();
                                    }
                                }
                            } else {
                                const Region<ModelVariant>* region = model->find_region(it["region"].as<std::string>());
                                if (region) {
                                    for (const auto& ea : region->economic_agents) {
                                        if (ea->type == EconomicAgent<ModelVariant>::Type::FIRM) {
                                            const Firm<ModelVariant>* p = ea->as_firm();
                                            internal_start_target("firms", p->sector, p->region);
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
                                const Sector<ModelVariant>* sector = model->find_sector(it["sector"].as<std::string>());
                                if (sector) {
                                    for (const auto& p : sector->firms_N) {
                                        internal_start_target("firms", p->sector, p->region);
                                        write_firm_parameters(p, it);
                                        internal_end_target();
                                    }
                                } else {
                                    warning("Sector " << it["sector"].as<std::string>() << " not found");
                                }
                            } else {
                                const Firm<ModelVariant>* p = model->find_firm(it["sector"].as<std::string>(), it["region"].as<std::string>());
                                if (p) {
                                    internal_start_target("firms", p->sector, p->region);
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
                            std::string sector_name, region_name;
                            getline(ss_l, sector_name, ':');
                            getline(ss_l, region_name, ':');
                            const Firm<ModelVariant>* p = model->find_firm(sector_name, region_name);
                            if (p) {
                                internal_start_target("firms", p->sector, p->region);
                                write_firm_parameters(p, it);
                                internal_end_target();
                            } else {
                                warning("Firm " << it["sector"].as<std::string>() << ":" << it["region"].as<std::string>() << " not found");
                            }
                        }
                    }
                } break;

                case settings::hstring::hash("consumer"): {
                    if (!it.has("region")) {
                        for (const auto& region : model->regions_R) {
                            for (const auto& ea : region->economic_agents) {
                                if (ea->type == EconomicAgent<ModelVariant>::Type::CONSUMER) {
                                    Consumer<ModelVariant>* c = ea->as_consumer();
                                    internal_start_target("consumers", c->region);
                                    write_consumer_parameters(c, it);
                                    internal_end_target();
                                }
                            }
                        }
                    } else {
                        Consumer<ModelVariant>* c = model->find_consumer(it["region"].as<std::string>());
                        if (c) {
                            internal_start_target("consumers", c->region);
                            write_consumer_parameters(c, it);
                            internal_end_target();
                        } else {
                            warning("Consumer " << it["region"].as<std::string>() << " not found");
                        }
                    }
                } break;

                case settings::hstring::hash("agent"): {
                    for (const auto& region : model->regions_R) {
                        for (const auto& ea : region->economic_agents) {
                            internal_start_target("agents", ea->sector, ea->region);
                            if (ea->type == EconomicAgent<ModelVariant>::Type::CONSUMER) {
                                write_consumer_parameters(ea->as_consumer(), it);
                            } else {
                                write_firm_parameters(ea->as_firm(), it);
                            }
                            internal_end_target();
                        }
                    }
                } break;

                case settings::hstring::hash("storage"): {
                    for (const auto& region : model->regions_R) {
                        for (const auto& ea : region->economic_agents) {
                            internal_start_target("storages", ea->sector, ea->region);
                            for (const auto& is : ea->input_storages) {
                                internal_start_target("storages", is->sector);
                                write_input_storage_parameters(is.get(), it);
                                internal_end_target();
                            }
                            internal_end_target();
                        }
                    }
                } break;

                case settings::hstring::hash("flow"): {
                    for (const auto& sector : model->sectors_C) {
                        for (const auto& ps : sector->firms_N) {
                            internal_start_target("flows", ps->sector, ps->region);
                            for (const auto& bc : ps->sales_manager->business_connections) {
                                internal_start_target("flows", bc->buyer->storage->economic_agent->sector, bc->buyer->storage->economic_agent->region);
                                write_connection_parameters(bc.get(), it);
                                internal_end_target();
                            }
                            internal_end_target();
                        }
                    }
                } break;

                case settings::hstring::hash("region"): {
                    const char* region_name = nullptr;
                    if (it.has("name")) {
                        region_name = it["name"].as<std::string>().c_str();
                    }
                    if (dynamic_cast<RasteredScenario<ModelVariant, FloatType>*>(scenario)) {
                        for (const auto& observable : it["parameters"].as_sequence()) {
                            const settings::hstring& name = observable.as<settings::hstring>();
                            switch (name) {
                                case settings::hstring::hash("total_current_proxy_sum"):
                                    for (const auto& forcing : static_cast<RasteredScenario<ModelVariant, FloatType>*>(scenario)->forcings()) {
                                        if (forcing.region && ((region_name == nullptr) || std::string(*forcing.region) == region_name)) {
                                            internal_start_target("regions", forcing.region);
                                            internal_write_value(name, forcing.forcing);
                                            internal_end_target();
                                        }
                                    }
                                    break;
                            }
                        }
                    }
                    for (const auto& region : model->regions_R) {
                        if ((region_name == nullptr) || std::string(*region) == region_name) {
                            internal_start_target("regions", region.get());
                            write_region_parameters(region.get(), it);
                            internal_end_target();
                        }
                    }
                } break;

                case settings::hstring::hash("sector"): {
                    const char* sector_name = nullptr;
                    if (it.has("sector")) {
                        sector_name = it["sector"].as<std::string>().c_str();
                    }
                    for (const auto& sector : model->sectors_C) {
                        if (std::string(*sector) == "FCON") {
                            continue;
                        }
                        if ((sector_name == nullptr) || std::string(*sector) == sector_name) {
                            internal_start_target("sectors", sector.get());
                            write_sector_parameters(sector.get(), it);
                            internal_end_target();
                        }
                    }
                } break;

                case settings::hstring::hash("meta"): {
                    internal_start_target("meta");
                    for (const auto& observable : it["parameters"].as_sequence()) {
                        const settings::hstring& name = observable.as<settings::hstring>();
                        switch (name) {
                            case settings::hstring::hash("duration"):
                                internal_write_value(name, Acclimate::instance()->duration());
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
    internal_iterate_end();
}

template<class ModelVariant>
void Output<ModelVariant>::end() {
    time_t end_time = time(nullptr);
    time_t duration = end_time - start_time;
    internal_write_footer(localtime(&duration));
    internal_end();
}

template<class ModelVariant>
void Output<ModelVariant>::internal_write_value(const std::string& name, const FloatType& v) {
    UNUSED(name);
    UNUSED(v);
}

template<class ModelVariant>
void Output<ModelVariant>::internal_write_header(tm* timestamp, int max_threads) {
    UNUSED(timestamp);
    UNUSED(max_threads);
}

template<class ModelVariant>
void Output<ModelVariant>::internal_write_footer(tm* duration) {
    UNUSED(duration);
}

template<class ModelVariant>
void Output<ModelVariant>::internal_write_settings() {}

template<class ModelVariant>
void Output<ModelVariant>::internal_start() {}

template<class ModelVariant>
void Output<ModelVariant>::internal_iterate_begin() {}

template<class ModelVariant>
void Output<ModelVariant>::internal_iterate_end() {}

template<class ModelVariant>
void Output<ModelVariant>::internal_end() {}

template<class ModelVariant>
void Output<ModelVariant>::internal_start_target(const std::string& name, Sector<ModelVariant>* sector, Region<ModelVariant>* region) {
    UNUSED(name);
    UNUSED(sector);
    UNUSED(region);
}

template<class ModelVariant>
void Output<ModelVariant>::internal_start_target(const std::string& name, Sector<ModelVariant>* sector) {
    UNUSED(name);
    UNUSED(sector);
}

template<class ModelVariant>
void Output<ModelVariant>::internal_start_target(const std::string& name, Region<ModelVariant>* region) {
    UNUSED(region);
    UNUSED(name);
}

template<class ModelVariant>
void Output<ModelVariant>::internal_start_target(const std::string& name) {
    UNUSED(name);
}

template<class ModelVariant>
void Output<ModelVariant>::internal_end_target() {}

template<class ModelVariant>
void Output<ModelVariant>::event(const EventType& type,
                                 const Sector<ModelVariant>* sector_from,
                                 const Region<ModelVariant>* region_from,
                                 const Sector<ModelVariant>* sector_to,
                                 const Region<ModelVariant>* region_to,
                                 const FloatType& value) {
    UNUSED(type);
    UNUSED(sector_from);
    UNUSED(region_from);
    UNUSED(sector_to);
    UNUSED(region_to);
    UNUSED(value);
}

template<class ModelVariant>
void Output<ModelVariant>::event(const EventType& type,
                                 const Sector<ModelVariant>* sector_from,
                                 const Region<ModelVariant>* region_from,
                                 const EconomicAgent<ModelVariant>* economic_agent_to,
                                 const FloatType& value) {
    event(type, sector_from, region_from, economic_agent_to == nullptr ? nullptr : economic_agent_to->sector,
          economic_agent_to == nullptr ? nullptr : economic_agent_to->region, value);
}

template<class ModelVariant>
void Output<ModelVariant>::event(const EventType& type,
                                 const EconomicAgent<ModelVariant>* economic_agent_from,
                                 const EconomicAgent<ModelVariant>* economic_agent_to,
                                 const FloatType& value) {
    event(type, economic_agent_from == nullptr ? nullptr : economic_agent_from->sector, economic_agent_from == nullptr ? nullptr : economic_agent_from->region,
          economic_agent_to == nullptr ? nullptr : economic_agent_to->as_firm()->sector, economic_agent_to == nullptr ? nullptr : economic_agent_to->region,
          value);
}

template<class ModelVariant>
void Output<ModelVariant>::event(const EventType& type,
                                 const EconomicAgent<ModelVariant>* economic_agent_from,
                                 const Sector<ModelVariant>* sector_to,
                                 const Region<ModelVariant>* region_to,
                                 const FloatType& value) {
    event(type, economic_agent_from == nullptr ? nullptr : economic_agent_from->sector, economic_agent_from == nullptr ? nullptr : economic_agent_from->region,
          sector_to, region_to, value);
}

INSTANTIATE_BASIC(Output);
}  // namespace acclimate
