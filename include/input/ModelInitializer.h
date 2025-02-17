// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_MODELINITIALIZER_H
#define ACCLIMATE_MODELINITIALIZER_H

#include "acclimate.h"
#include "settingsnode.h"

namespace acclimate {

class Consumer;
class EconomicAgent;
class Firm;
class Model;
class Region;
class Sector;

class ModelInitializer {
  private:
    non_owning_ptr<Model> model_;
    const settings::SettingsNode& settings_;

  private:
    settings::SettingsNode get_firm_property(const std::string& name,
                                             const std::string& sector_name,
                                             const std::string& region_name,
                                             const std::string& property_name) const;
    settings::SettingsNode get_consumer_property(const std::string& name, const std::string& region_name, const std::string& property_name) const;
    static settings::SettingsNode get_named_property(const settings::SettingsNode& node_settings,
                                                     const std::string& node_name,
                                                     const std::string& property_name);
    Sector* add_sector(const std::string& name);
    Region* add_region(const std::string& name);
    Firm* add_firm(const std::string& name, Sector* sector, Region* region);
    Consumer* add_consumer(std::string name, Region* region);
    EconomicAgent* add_standard_agent(Sector* sector, Region* region);
    void create_simple_transport_connection(Region* region_from, Region* region_to, TransportDelay transport_delay);
    void initialize_connection(Firm* firm_from, EconomicAgent* economic_agent_to, const Flow& flow);
    void clean_network();
    void pre_initialize();
    void post_initialize();
    void build_agent_network();
    void build_artificial_network();
    void build_transport_network();
    void read_transport_times_csv(const std::string& index_filename, const std::string& filename);
    void read_centroids_netcdf(const std::string& filename);
    void read_transport_network_netcdf(const std::string& filename);

  public:
    ModelInitializer(Model* model_p, const settings::SettingsNode& settings_p);
    void initialize();

    void debug_print_network_characteristics() const;

    const Model* model() const { return model_; }
    Model* model() { return model_; }
    std::string name() const { return "MODELINITIALIZER"; }
};
}  // namespace acclimate

#endif
