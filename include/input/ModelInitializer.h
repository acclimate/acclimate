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

#ifndef ACCLIMATE_MODELINITIALIZER_H
#define ACCLIMATE_MODELINITIALIZER_H

#include <string>

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
    non_owning_ptr<Model> model_m;
    const settings::SettingsNode& settings;

  private:
    settings::SettingsNode get_firm_property(const std::string& name,
                                             const std::string& sector_name,
                                             const std::string& region_name,
                                             const std::string& property_name) const;

    settings::SettingsNode get_consumer_property(const std::string& name, const std::string& region_name, const std::string& property_name) const;

    settings::SettingsNode get_named_property(const settings::SettingsNode& node_settings,
                                              const std::string& node_name,
                                              const std::string& property_name) const;
    Sector* add_sector(const std::string& name);
    Region* add_region(const std::string& name);
    Firm* add_firm(std::string name, Sector* sector, Region* region);
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

    const Model* model() const { return model_m; }
    Model* model() { return model_m; }
    std::string name() const { return "MODELINITIALIZER"; }
};
}  // namespace acclimate

#endif
