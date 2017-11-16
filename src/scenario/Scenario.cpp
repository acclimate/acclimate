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

#include "scenario/Scenario.h"
#include "model/EconomicAgent.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Scenario<ModelVariant>::Scenario(const settings::SettingsNode& settings_p, const Model<ModelVariant>* model_p) : model(model_p), settings(settings_p) {
    srand(0);
}

template<class ModelVariant>
Time Scenario<ModelVariant>::start() {
    if (settings["scenario"].has("start")) {
        start_time = settings["scenario"]["start"].template as<Time>();
    }
    stop_time = settings["scenario"]["stop"].template as<Time>();
    if (settings["scenario"].has("seed")) {
        srand(settings["scenario"]["seed"].template as<unsigned int>());
    }
    return start_time;
}

template<class ModelVariant>
void Scenario<ModelVariant>::set_firm_property(Firm<ModelVariant>* firm, const settings::SettingsNode& node, const bool reset) {
    for (const auto& it_map : node.as_map()) {
        const std::string& name = it_map.first;
        const settings::SettingsNode& it = it_map.second;
        if (name == "remaining_capacity") {
            firm->forcing(reset ? 1.0 : it.as<Forcing>() / firm->capacity_manager->possible_overcapacity_ratio_beta);
        } else if (name == "forcing") {
            firm->forcing(reset ? Forcing(1.0) : it.as<Forcing>());
        }
    }
}

template<class ModelVariant>
void Scenario<ModelVariant>::set_consumer_property(Consumer<ModelVariant>* consumer, const settings::SettingsNode& node, const bool reset) {
    for (const auto& it_map : node.as_map()) {
        const std::string& name = it_map.first;
        const settings::SettingsNode& it = it_map.second;
        if (name == "remaining_consumption_rate") {
            consumer->forcing(reset ? Forcing(1.0) : it.as<Forcing>());
        }
    }
}

template<class ModelVariant>
void Scenario<ModelVariant>::apply_target(const settings::SettingsNode& node, const bool reset) {
    for (const auto& targets : node.as_sequence()) {
        for (const auto& target : targets.as_map()) {
            const std::string& type = target.first;
            const settings::SettingsNode& it = target.second;
            if (type == "firm") {
                if (it.has("sector")) {
                    if (it.has("region")) {
                        Firm<ModelVariant>* firm = model->find_firm(it["sector"].template as<std::string>(), it["region"].template as<std::string>());
                        if (firm) {
                            set_firm_property(firm, it, reset);
                        } else {
                            error("Firm " << it["sector"].template as<std::string>() << ":" << it["region"].template as<std::string>() << " not found");
                        }
                    } else {
                        Sector<ModelVariant>* sector = model->find_sector(it["sector"].template as<std::string>());
                        if (sector) {
                            for (auto& p : sector->firms_N) {
                                set_firm_property(p, it, reset);
                            }
                        } else {
                            error("Sector " << it["sector"].template as<std::string>() << " not found");
                        }
                    }
                } else {
                    if (it.has("region")) {
                        Region<ModelVariant>* region = model->find_region(it["region"].template as<std::string>());
                        if (region) {
                            for (auto& ea : region->economic_agents) {
                                if (ea->type == EconomicAgent<ModelVariant>::Type::FIRM) {
                                    set_firm_property(ea->as_firm(), it, reset);
                                }
                            }
                        } else {
                            error("Region " << it["region"].template as<std::string>() << " not found");
                        }
                    } else {
                        for (auto& s : model->sectors_C) {
                            for (auto& p : s->firms_N) {
                                set_firm_property(p, it, reset);
                            }
                        }
                    }
                }
            } else if (type == "consumer") {
                if (it.has("region")) {
                    Consumer<ModelVariant>* consumer = model->find_consumer(it["region"].template as<std::string>());
                    if (consumer) {
                        set_consumer_property(consumer, it, reset);
                    } else {
                        error("Consumer " << it["region"].template as<std::string>() << " not found");
                    }
                } else {
                    for (auto& r : model->regions_R) {
                        for (auto& ea : r->economic_agents) {
                            if (ea->type == EconomicAgent<ModelVariant>::Type::CONSUMER) {
                                set_consumer_property(ea->as_consumer(), it, reset);
                            }
                        }
                    }
                }
            }
        }
    }
}

template<class ModelVariant>
bool Scenario<ModelVariant>::iterate() {
    if (model->time() > stop_time) {
        return false;
    }

    for (const auto& event : settings["scenario"]["events"].as_sequence()) {
        const std::string& type = event["type"].template as<std::string>();
        if (type == "shock") {
            const Time from = event["from"].template as<Time>();
            const Time to = event["to"].template as<Time>();
            if (model->time() >= from && model->time() <= to) {
                apply_target(event["targets"], false);
            } else if (model->time() == to + model->delta_t()) {
                apply_target(event["targets"], true);
            }
        }
    }
    return true;
}

template<class ModelVariant>
std::string Scenario<ModelVariant>::time_units_str() const {
    if (settings["scenario"].has("baseyear")) {
        return std::string("days since ") + std::to_string(settings["scenario"]["baseyear"].template as<unsigned int>()) + "-1-1";
    }
    return "days since 0-1-1";
}

INSTANTIATE_BASIC(Scenario);
}  // namespace acclimate
