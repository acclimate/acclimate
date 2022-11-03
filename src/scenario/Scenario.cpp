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

#include "scenario/Scenario.h"

#include <cstdlib>
#include <memory>
#include <utility>

#include "acclimate.h"
#include "model/CapacityManager.h"
#include "model/Consumer.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/GeoLocation.h"
#include "model/Model.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "settingsnode.h"

namespace acclimate {

Scenario::Scenario(const settings::SettingsNode& settings_p, settings::SettingsNode scenario_node_p, Model* model_p)
    : model_m(model_p), scenario_node(std::move(scenario_node_p)), settings(settings_p) {
    srand(0);  // NOLINT(cert-msc32-c,cert-msc51-cpp)
}

void Scenario::set_firm_property(Firm* firm, const settings::SettingsNode& node, bool reset) {
    for (const auto& it_map : node.as_map()) {
        const std::string& name = it_map.first;
        const settings::SettingsNode& it = it_map.second;
        if (name == "remaining_capacity") {
            firm->set_forcing(reset ? 1.0 : it.as<Forcing>() / firm->capacity_manager->possible_overcapacity_ratio_beta);
        } else if (name == "forcing") {
            firm->set_forcing(reset ? Forcing(1.0) : it.as<Forcing>());
        }
    }
}

void Scenario::set_consumer_property(Consumer* consumer, const settings::SettingsNode& node, bool reset) {
    for (const auto& it_map : node.as_map()) {
        const std::string& name = it_map.first;
        const settings::SettingsNode& it = it_map.second;
        if (name == "remaining_consumption_rate") {
            consumer->set_forcing(reset ? Forcing(1.0) : it.as<Forcing>());
        }
    }
}

void Scenario::set_location_property(GeoLocation* location, const settings::SettingsNode& node, bool reset) {
    for (const auto& it_map : node.as_map()) {
        const std::string& name = it_map.first;
        const settings::SettingsNode& it = it_map.second;
        if (name == "passage") {
            location->set_forcing_nu(reset ? -1. : it.as<Forcing>());
        }
    }
}

void Scenario::apply_target(const settings::SettingsNode& node, bool reset) {
    for (const auto& targets : node.as_sequence()) {
        for (const auto& target : targets.as_map()) {
            const std::string& type = target.first;
            const settings::SettingsNode& it = target.second;
            if (type == "firm") {
                if (it.has("id")) {
                    const auto agent_name = it["id"].as<std::string>();
                    auto* agent = model()->economic_agents.find(agent_name);
                    if (agent == nullptr) {
                        throw log::error(this, "Agent ", agent_name, " not found");
                    }
                    if (!agent->is_firm()) {
                        throw log::error(this, "Agent ", agent_name, " is not a firm");
                    }
                    set_firm_property(agent->as_firm(), it, reset);
                } else if (it.has("sector")) {
                    auto* sector = model()->sectors.find(it["sector"].as<std::string>());
                    if (sector == nullptr) {
                        throw log::error(this, "Sector ", it["sector"].as<std::string>(), " not found");
                    }
                    if (it.has("region")) {
                        Firm* firm = sector->firms.find(it["sector"].as<std::string>() + ":" + it["region"].as<std::string>());
                        if (firm == nullptr) {
                            throw log::error(this, "Firm ", it["sector"].as<std::string>(), ":", it["region"].as<std::string>(), " not found");
                        }
                        set_firm_property(firm, it, reset);
                    } else {
                        for (auto& p : sector->firms) {
                            set_firm_property(p, it, reset);
                        }
                    }
                } else {
                    if (it.has("region")) {
                        Region* region = model()->regions.find(it["region"].as<std::string>());
                        if (region == nullptr) {
                            throw log::error(this, "Region ", it["region"].as<std::string>(), " not found");
                        }
                        for (auto& ea : region->economic_agents) {
                            if (ea->type == EconomicAgent::type_t::FIRM) {
                                set_firm_property(ea->as_firm(), it, reset);
                            }
                        }
                    } else {
                        for (auto& s : model()->sectors) {
                            for (auto& p : s->firms) {
                                set_firm_property(p, it, reset);
                            }
                        }
                    }
                }
            } else if (type == "consumer") {
                if (it.has("region")) {
                    Region* region = model()->regions.find(it["region"].as<std::string>());
                    if (region == nullptr) {
                        throw log::error(this, "Region ", it["region"].as<std::string>(), " not found");
                    }
                    for (auto& ea : region->economic_agents) {
                        if (ea->type == EconomicAgent::type_t::CONSUMER) {
                            set_consumer_property(ea->as_consumer(), it, reset);
                        }
                    }
                } else {
                    for (auto& r : model()->regions) {
                        for (auto& ea : r->economic_agents) {
                            if (ea->type == EconomicAgent::type_t::CONSUMER) {
                                set_consumer_property(ea->as_consumer(), it, reset);
                            }
                        }
                    }
                }
            } else if (type == "sea") {
                if (it.has("sea_route")) {
                    auto* location = model()->other_locations.find(it["sea_route"].as<std::string>());
                    if (location == nullptr) {
                        throw log::error(this, "Sea route ", it["sea_route"].as<std::string>(), " not found");
                    }
                    set_location_property(location, it, reset);
                }
            }
        }
    }
}

void Scenario::iterate() {
    for (const auto& event : scenario_node["events"].as_sequence()) {
        const std::string& type = event["type"].as<std::string>();
        if (type == "shock") {
            const Time from = event["from"].as<Time>();
            const Time to = event["to"].as<Time>();
            if (model()->time() >= from && model()->time() <= to) {
                apply_target(event["targets"], false);
            } else if (model()->time() == to + model()->delta_t()) {
                apply_target(event["targets"], true);
            }
        }
    }
}

std::string Scenario::time_units_str() const {
    if (scenario_node.has("basedate")) {
        return std::string("days since ") + scenario_node["basedate"].as<std::string>("2000-1-1");
    }
    return "days since 0-1-1";
}

}  // namespace acclimate
