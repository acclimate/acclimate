// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "output/ArrayOutput.h"

#include <set>

#include "ModelRun.h"
#include "model/BusinessConnection.h"
#include "model/Consumer.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/PurchasingManager.h"
#include "model/Region.h"
#include "model/SalesManager.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "model/TransportChainLink.h"
#include "settingsnode.h"

namespace acclimate {

class CollectVariables {
  public:
    struct H {
        static constexpr auto hash(const char* s) -> const char* { return s; }
    };

  private:
    bool want_all_;
    std::set<std::string> wanted_variables_;
    std::vector<ArrayOutput::Variable>& variables_;

    void add_variable(const std::string& name, bool flow_or_stock) {
        const auto name_hash = hash(name.c_str());
        if (flow_or_stock) {
            variables_.emplace_back(name + "_quantity", name_hash);
            variables_.emplace_back(name + "_value", name_hash);
        } else {
            variables_.emplace_back(name, name_hash);
        }
    }

    auto collect_variable(const char* name, bool flow_or_stock) -> bool {
        if (want_all_) {
            add_variable(name, flow_or_stock);
            return true;
        }
        const auto num_removed = wanted_variables_.erase(name);
        if (num_removed > 0) {
            add_variable(name, flow_or_stock);
        }
        return !wanted_variables_.empty();
    }

  public:
    CollectVariables(const settings::SettingsNode& obs_node, std::vector<ArrayOutput::Variable>& variables_p) : variables_(variables_p) {
        if (obs_node.has("output")) {
            const auto vec = obs_node["output"].to_vector<std::string>();
            std::copy(std::begin(vec), std::end(vec), std::inserter(wanted_variables_, std::end(wanted_variables_)));
            want_all_ = false;
        } else {
            want_all_ = true;
        }
    }

    template<typename T>
    void collect() {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull"
#endif
        static_cast<T*>(nullptr)->template observe<CollectVariables, CollectVariables::H>(*this);  // NOLINT(clang-analyzer-core.CallAndMessage)
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        if (!wanted_variables_.empty()) {
            for (const auto& name : wanted_variables_) {
                throw log::error("Unknown observable variable '", name, "'");
            }
        }
    }

    template<typename Function>
    auto set(const char* name, Function&& f) ->
        typename std::enable_if<!std::is_same<decltype(f()), Flow>::value && !std::is_same<decltype(f()), Stock>::value, bool>::type {
        return collect_variable(name, false);
    }

    template<typename Function>
    auto set(const char* name, Function&& f) ->
        typename std::enable_if<std::is_same<decltype(f()), Flow>::value || std::is_same<decltype(f()), Stock>::value, bool>::type {
        return collect_variable(name, true);
    }
};

class WriteVariables {
  public:
    struct H {
        static constexpr auto hash(const char* s) -> hash_t { return acclimate::hash(s); }
    };

  private:
    std::size_t index_{};
    std::vector<ArrayOutput::Variable>::iterator var_;
    std::vector<ArrayOutput::Variable>& variables_;

  public:
    explicit WriteVariables(std::vector<ArrayOutput::Variable>& variables_p) : variables_(variables_p) {}

    template<typename T>
    void collect(const T* v, std::size_t index) {
        index_ = index;
        var_ = std::begin(variables_);
        v->template observe<WriteVariables, WriteVariables::H>(*this);
    }

    template<typename Function>
    auto set(hash_t name_hash, Function&& f) ->
        typename std::enable_if<!std::is_same<decltype(f()), Flow>::value && !std::is_same<decltype(f()), Stock>::value, bool>::type {
        if (var_->name_hash == name_hash) {
            var_->data[index_] = to_float(f());
            ++var_;
            return var_ != std::end(variables_);
        }
        return true;
    }

    template<typename Function>
    auto set(hash_t name_hash, Function&& f) ->
        typename std::enable_if<std::is_same<decltype(f()), Flow>::value || std::is_same<decltype(f()), Stock>::value, bool>::type {
        if (var_->name_hash == name_hash) {
            const auto& v = f();
            var_->data[index_] = to_float(v.get_quantity());
            ++var_;
            var_->data[index_] = to_float(v.get_value());
            ++var_;
            return var_ != std::end(variables_);
        }
        return true;
    }
};

template<std::size_t dim>
void ArrayOutput::resize_data(Observable<dim>& obs) {
    auto size = only_current_timestep_ ? 1 : model()->run()->total_timestep_count();
    if constexpr (dim > 0) {
        for (std::size_t i = 0; i < dim; ++i) {
            size *= obs.sizes[i];
        }
    }
    for (auto& var_ : obs.variables) {
        var_.data.resize(size, std::numeric_limits<output_float_t>::quiet_NaN());
    }
}

ArrayOutput::ArrayOutput(Model* model_p, const settings::SettingsNode& settings, bool only_current_timestep_p)
    : Output(model_p), only_current_timestep_(only_current_timestep_p) {
    include_events_ = settings["events_"].as<bool>(false);

    // model
    if (const auto& obs_node = settings["model"]; !obs_node.empty()) {
        CollectVariables collector(obs_node, obs_model_.variables);
        collector.collect<Model>();
        resize_data(obs_model_);
    }

    // firms
    if (const auto& obs_node = settings["firms"]; !obs_node.empty()) {
        if (const auto& node = obs_node["select_firm"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* agent = model()->economic_agents.find(hash_t(name.as<hashed_string>()));
                if (agent == nullptr) {
                    throw log::error(this, "Firm ", name, " not found");
                }
                if (!agent->is_firm()) {
                    throw log::error(this, "Agent ", name, " is not a firm");
                }
                obs_firms_.indices[0].push_back(agent->id.index());
            }
        }
        if (const auto& node = obs_node["select_sector"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* sector = model()->sectors.find(hash_t(name.as<hashed_string>()));
                if (sector == nullptr) {
                    throw log::error(this, "Sector ", name, " not found");
                }
                for (const auto* firm : sector->firms) {
                    obs_firms_.indices[0].push_back(firm->id.index());
                }
            }
        }
        if (const auto& node = obs_node["select_region"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* region = model()->regions.find(hash_t(name.as<hashed_string>()));
                if (region == nullptr) {
                    throw log::error(this, "Region ", name, " not found");
                }
                for (const auto* ea : region->economic_agents) {
                    if (ea->is_firm()) {
                        obs_firms_.indices[0].push_back(ea->id.index());
                    }
                }
            }
        }
        if (obs_firms_.indices[0].empty()) {
            obs_firms_.sizes[0] = model()->economic_agents.size();
        } else {
            obs_firms_.sizes[0] = obs_firms_.indices[0].size();
        }
        CollectVariables collector(obs_node, obs_firms_.variables);
        collector.collect<Firm>();
        resize_data(obs_firms_);
    }

    // consumers
    if (const auto& obs_node = settings["consumers"]; !obs_node.empty()) {
        if (const auto& node = obs_node["select_consumer"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* agent = model()->economic_agents.find(hash_t(name.as<hashed_string>()));
                if (agent == nullptr) {
                    throw log::error(this, "Consumer ", name, " not found");
                }
                if (!agent->is_consumer()) {
                    throw log::error(this, "Agent ", name, " is not a consumer");
                }
                obs_consumers_.indices[0].push_back(agent->id.index());
            }
        }
        if (const auto& node = obs_node["select_region"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* region = model()->regions.find(hash_t(name.as<hashed_string>()));
                if (region == nullptr) {
                    throw log::error(this, "Region ", name, " not found");
                }
                for (const auto* ea : region->economic_agents) {
                    if (ea->is_consumer()) {
                        obs_consumers_.indices[0].push_back(ea->id.index());
                    }
                }
            }
        }
        if (obs_consumers_.indices[0].empty()) {
            obs_consumers_.sizes[0] = model()->economic_agents.size();
        } else {
            obs_consumers_.sizes[0] = obs_consumers_.indices[0].size();
        }
        CollectVariables collector(obs_node, obs_consumers_.variables);
        collector.collect<Consumer>();
        resize_data(obs_consumers_);
    }

    // sectors
    if (const auto& obs_node = settings["sectors"]; !obs_node.empty()) {
        if (const auto& node = obs_node["select_sector"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* sector = model()->sectors.find(hash_t(name.as<hashed_string>()));
                if (sector == nullptr) {
                    throw log::error(this, "Sector ", name, " not found");
                }
                obs_sectors_.indices[0].push_back(sector->id.index());
            }
        }
        if (obs_sectors_.indices[0].empty()) {
            obs_sectors_.sizes[0] = model()->sectors.size();
        } else {
            obs_sectors_.sizes[0] = obs_sectors_.indices[0].size();
        }
        CollectVariables collector(obs_node, obs_sectors_.variables);
        collector.collect<Sector>();
        resize_data(obs_sectors_);
    }

    // regions
    if (const auto& obs_node = settings["regions"]; !obs_node.empty()) {
        if (const auto& node = obs_node["select_region"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* region = model()->regions.find(hash_t(name.as<hashed_string>()));
                if (region == nullptr) {
                    throw log::error(this, "Region ", name, " not found");
                }
                obs_regions_.indices[0].push_back(region->id.index());
            }
        }
        if (obs_regions_.indices[0].empty()) {
            obs_regions_.sizes[0] = model()->regions.size();
        } else {
            obs_regions_.sizes[0] = obs_regions_.indices[0].size();
        }
        CollectVariables collector(obs_node, obs_regions_.variables);
        collector.collect<Region>();
        resize_data(obs_regions_);
    }

    // locations
    if (const auto& obs_node = settings["locations"]; !obs_node.empty()) {
        if (const auto& node = obs_node["select_location"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* location = model()->other_locations.find(hash_t(name.as<hashed_string>()));
                if (location == nullptr) {
                    throw log::error(this, "Location ", name, " not found");
                }
                obs_locations_.indices[0].push_back(location->id.index());
            }
        }
        if (obs_locations_.indices[0].empty()) {
            obs_locations_.sizes[0] = model()->other_locations.size();
        } else {
            obs_locations_.sizes[0] = obs_locations_.indices[0].size();
        }
        CollectVariables collector(obs_node, obs_locations_.variables);
        collector.collect<GeoLocation>();
        resize_data(obs_locations_);
    }

    // storages
    if (const auto& obs_node = settings["storages"]; !obs_node.empty()) {
        if (const auto& node = obs_node["select_sector"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* sector = model()->sectors.find(hash_t(name.as<hashed_string>()));
                if (sector == nullptr) {
                    throw log::error(this, "Sector ", name, " not found");
                }
                obs_storages_.indices[0].push_back(sector->id.index());
            }
        }
        if (const auto& node = obs_node["select_agent"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* agent = model()->economic_agents.find(hash_t(name.as<hashed_string>()));
                if (agent == nullptr) {
                    throw log::error(this, "Agent ", name, " not found");
                }
                obs_storages_.indices[1].push_back(agent->id.index());
            }
        }
        if (const auto& node = obs_node["select_agent_sector"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* sector = model()->sectors.find(hash_t(name.as<hashed_string>()));
                if (sector == nullptr) {
                    throw log::error(this, "Sector ", name, " not found");
                }
                for (const auto* firm : sector->firms) {
                    obs_storages_.indices[1].push_back(firm->id.index());
                }
            }
        }
        if (const auto& node = obs_node["select_agent_region"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* region = model()->regions.find(hash_t(name.as<hashed_string>()));
                if (region == nullptr) {
                    throw log::error(this, "Region ", name, " not found");
                }
                for (const auto* ea : region->economic_agents) {
                    obs_storages_.indices[1].push_back(ea->id.index());
                }
            }
        }
        if (obs_storages_.indices[0].empty()) {
            obs_storages_.sizes[0] = model()->sectors.size();
        } else {
            obs_storages_.sizes[0] = obs_storages_.indices[0].size();
            throw log::error(this, "Selection on storage sector not supported yet");
        }
        if (obs_storages_.indices[1].empty()) {
            obs_storages_.sizes[1] = model()->economic_agents.size();
        } else {
            obs_storages_.sizes[1] = obs_storages_.indices[1].size();
        }
        CollectVariables collector(obs_node, obs_storages_.variables);
        collector.collect<Storage>();
        resize_data(obs_storages_);
    }

    // flows
    if (const auto& obs_node = settings["flows"]; !obs_node.empty()) {
        if (const auto& node = obs_node["select_firm_from"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* agent = model()->economic_agents.find(hash_t(name.as<hashed_string>()));
                if (agent == nullptr) {
                    throw log::error(this, "Firm ", name, " not found");
                }
                if (!agent->is_firm()) {
                    throw log::error(this, "Agent ", name, " is not a firm");
                }
                obs_flows_.indices[0].push_back(agent->id.index());
            }
        }
        if (const auto& node = obs_node["select_sector_from"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* sector = model()->sectors.find(hash_t(name.as<hashed_string>()));
                if (sector == nullptr) {
                    throw log::error(this, "Sector ", name, " not found");
                }
                for (const auto* firm : sector->firms) {
                    obs_flows_.indices[0].push_back(firm->id.index());
                }
            }
        }
        if (const auto& node = obs_node["select_region_from"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* region = model()->regions.find(hash_t(name.as<hashed_string>()));
                if (region == nullptr) {
                    throw log::error(this, "Region ", name, " not found");
                }
                for (const auto* ea : region->economic_agents) {
                    if (ea->is_firm()) {
                        obs_flows_.indices[0].push_back(ea->id.index());
                    }
                }
            }
        }
        if (const auto& node = obs_node["select_agent_to"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* agent = model()->economic_agents.find(hash_t(name.as<hashed_string>()));
                if (agent == nullptr) {
                    throw log::error(this, "Agent ", name, " not found");
                }
                obs_flows_.indices[1].push_back(agent->id.index());
            }
        }
        if (const auto& node = obs_node["select_sector_to"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* sector = model()->sectors.find(hash_t(name.as<hashed_string>()));
                if (sector == nullptr) {
                    throw log::error(this, "Sector ", name, " not found");
                }
                for (const auto* firm : sector->firms) {
                    obs_flows_.indices[1].push_back(firm->id.index());
                }
            }
        }
        if (const auto& node = obs_node["select_region_to"]; !node.empty()) {
            for (const auto& name : node.as_sequence()) {
                const auto* region = model()->regions.find(hash_t(name.as<hashed_string>()));
                if (region == nullptr) {
                    throw log::error(this, "Region ", name, " not found");
                }
                for (const auto* ea : region->economic_agents) {
                    obs_flows_.indices[1].push_back(ea->id.index());
                }
            }
        }
        if (obs_flows_.indices[0].empty()) {
            obs_flows_.sizes[0] = model()->economic_agents.size();
        } else {
            obs_flows_.sizes[0] = obs_flows_.indices[0].size();
        }
        if (obs_flows_.indices[1].empty()) {
            obs_flows_.sizes[1] = model()->economic_agents.size();
        } else {
            obs_flows_.sizes[1] = obs_flows_.indices[1].size();
        }
        if (!obs_flows_.indices[0].empty() && !obs_flows_.indices[1].empty()) {
            throw log::error(this, "Selection on both source agent and target agent not supported yet");
        }
        CollectVariables collector(obs_node, obs_flows_.variables);
        collector.collect<BusinessConnection>();
        resize_data(obs_flows_);
    }
}

void ArrayOutput::iterate() {
    const auto t = model()->timestep();

    // model
    if (!obs_model_.variables.empty()) {
        WriteVariables collector(obs_model_.variables);
        const auto offset = only_current_timestep_ ? 0 : t;
        collector.collect(model(), offset);
    }

    // firms
    if (!obs_firms_.variables.empty()) {
        WriteVariables collector(obs_firms_.variables);
        const auto& vec = model()->economic_agents;
        const auto& indices = obs_firms_.indices[0];
        const auto offset = only_current_timestep_ ? 0 : t * obs_firms_.sizes[0];
        if (indices.empty()) {
            for (std::size_t i = 0; i < vec.size(); ++i) {
                if (vec[i]->is_firm()) {
                    collector.collect(vec[i]->as_firm(), offset + i);
                }
            }
        } else {
            for (std::size_t i = 0; i < indices.size(); ++i) {
                if (vec[indices[i]]->is_firm()) {
                    collector.collect(vec[indices[i]]->as_firm(), offset + i);
                }
            }
        }
    }

    // consumers
    if (!obs_consumers_.variables.empty()) {
        WriteVariables collector(obs_consumers_.variables);
        const auto& vec = model()->economic_agents;
        const auto& indices = obs_consumers_.indices[0];
        const auto offset = only_current_timestep_ ? 0 : t * obs_consumers_.sizes[0];
        if (indices.empty()) {
            for (std::size_t i = 0; i < vec.size(); ++i) {
                if (vec[i]->is_consumer()) {
                    collector.collect(vec[i]->as_consumer(), offset + i);
                }
            }
        } else {
            for (std::size_t i = 0; i < indices.size(); ++i) {
                if (vec[indices[i]]->is_consumer()) {
                    collector.collect(vec[indices[i]]->as_consumer(), offset + i);
                }
            }
        }
    }

    // sectors
    if (!obs_sectors_.variables.empty()) {
        WriteVariables collector(obs_sectors_.variables);
        const auto& vec = model()->sectors;
        const auto& indices = obs_sectors_.indices[0];
        const auto offset = only_current_timestep_ ? 0 : t * obs_sectors_.sizes[0];
        if (indices.empty()) {
            for (std::size_t i = 0; i < vec.size(); ++i) {
                collector.collect(vec[i], offset + i);
            }
        } else {
            for (std::size_t i = 0; i < indices.size(); ++i) {
                collector.collect(vec[indices[i]], offset + i);
            }
        }
    }

    // regions
    if (!obs_regions_.variables.empty()) {
        WriteVariables collector(obs_regions_.variables);
        const auto& vec = model()->regions;
        const auto& indices = obs_regions_.indices[0];
        const auto offset = only_current_timestep_ ? 0 : t * obs_regions_.sizes[0];
        if (indices.empty()) {
            for (std::size_t i = 0; i < vec.size(); ++i) {
                collector.collect(vec[i], offset + i);
            }
        } else {
            for (std::size_t i = 0; i < indices.size(); ++i) {
                collector.collect(vec[indices[i]], offset + i);
            }
        }
    }

    // locations
    if (!obs_locations_.variables.empty()) {
        WriteVariables collector(obs_locations_.variables);
        const auto& vec = model()->other_locations;
        const auto& indices = obs_locations_.indices[0];
        const auto offset = only_current_timestep_ ? 0 : t * obs_locations_.sizes[0];
        if (indices.empty()) {
            for (std::size_t i = 0; i < vec.size(); ++i) {
                collector.collect(vec[i], offset + i);
            }
        } else {
            for (std::size_t i = 0; i < indices.size(); ++i) {
                collector.collect(vec[indices[i]], offset + i);
            }
        }
    }

    // storages
    if (!obs_storages_.variables.empty()) {
        WriteVariables collector(obs_storages_.variables);
        const auto& vec = model()->economic_agents;
        const auto& indices = obs_storages_.indices[1];  // selection on storage sector not supported yet
        const auto offset = only_current_timestep_ ? 0 : t * obs_storages_.sizes[1] * obs_storages_.sizes[0];
        if (indices.empty()) {
            for (std::size_t i = 0; i < vec.size(); ++i) {
                const auto* agent = vec[i];
                for (const auto& storage : agent->input_storages) {
                    collector.collect(storage.get(), offset + storage->sector->id.index() * obs_storages_.sizes[1] + i);
                }
            }
        } else {
            for (std::size_t i = 0; i < indices.size(); ++i) {
                const auto* agent = vec[indices[i]];
                for (const auto& storage : agent->input_storages) {
                    collector.collect(storage.get(), offset + storage->sector->id.index() * obs_storages_.sizes[1] + i);
                }
            }
        }
    }

    // flows
    if (!obs_flows_.variables.empty()) {
        WriteVariables collector(obs_flows_.variables);
        const auto& vec = model()->economic_agents;
        const auto offset = only_current_timestep_ ? 0 : t * obs_flows_.sizes[1] * obs_flows_.sizes[0];
        if (obs_flows_.indices[0].empty()) {
            const auto& indices = obs_flows_.indices[1];
            if (indices.empty()) {
                for (std::size_t i = 0; i < vec.size(); ++i) {
                    const auto* agent = vec[i];
                    if (agent->is_firm()) {
                        const auto n = offset + i * obs_flows_.sizes[1];
                        for (const auto& bc : agent->as_firm()->sales_manager->business_connections) {
                            collector.collect(bc.get(), n + bc->buyer->storage->economic_agent->id.index());
                        }
                    }
                }
            } else {
                for (std::size_t i = 0; i < indices.size(); ++i) {
                    const auto* agent = vec[indices[i]];
                    for (const auto& is : agent->input_storages) {
                        for (const auto& bc : is->purchasing_manager->business_connections) {
                            collector.collect(bc.get(), offset + bc->seller->firm->id.index() * obs_flows_.sizes[1] + i);
                        }
                    }
                }
            }
        } else {
            assert(obs_flows_.indices[1].empty());  // selection on flow sector not supported yet
            const auto& indices = obs_flows_.indices[0];
            for (std::size_t i = 0; i < indices.size(); ++i) {
                const auto* agent = vec[indices[i]];
                if (agent->is_firm()) {
                    const auto n = offset + i * obs_flows_.sizes[1];
                    for (const auto& bc : agent->as_firm()->sales_manager->business_connections) {
                        collector.collect(bc.get(), n + bc->buyer->storage->economic_agent->id.index());
                    }
                }
            }
        }
    }
}

void ArrayOutput::event(EventType type, const Sector* sector, const EconomicAgent* economic_agent, FloatType value) {
    if (include_events_) {
        Event ev;
        ev.time = model()->timestep();
        ev.type = static_cast<unsigned char>(type);
        ev.index1 = sector->id.index();
        ev.index2 = economic_agent->id.index();
        ev.value = value;
        event_lock_.call([&]() { events_.emplace_back(ev); });
    }
}

void ArrayOutput::event(EventType type, const EconomicAgent* economic_agent, FloatType value) {
    if (include_events_) {
        Event ev;
        ev.time = model()->timestep();
        ev.type = static_cast<unsigned char>(type);
        ev.index1 = economic_agent->id.index();
        ev.value = value;
        event_lock_.call([&]() { events_.emplace_back(ev); });
    }
}

void ArrayOutput::event(EventType type, const EconomicAgent* economic_agent_from, const EconomicAgent* economic_agent_to, FloatType value) {
    if (include_events_) {
        Event ev;
        ev.time = model()->timestep();
        ev.type = static_cast<unsigned char>(type);
        ev.index1 = economic_agent_from->id.index();
        ev.index2 = economic_agent_to->id.index();
        ev.value = value;
        event_lock_.call([&]() { events_.emplace_back(ev); });
    }
}

}  // namespace acclimate
