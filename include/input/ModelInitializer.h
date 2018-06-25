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

#ifndef ACCLIMATE_MODELINITIALIZER_H
#define ACCLIMATE_MODELINITIALIZER_H

#include <string>
#include <unordered_map>
#include "model/GeographicPoint.h"
#include "settingsnode.h"
#include "types.h"

namespace acclimate {

template<class ModelVariant>
class Consumer;
template<class ModelVariant>
class EconomicAgent;
template<class ModelVariant>
class Model;
template<class ModelVariant>
class Firm;
template<class ModelVariant>
class Region;
template<class ModelVariant>
class Sector;

template<class ModelVariant>
class ModelInitializer {
  private:
    settings::SettingsNode get_firm_property(const std::string& sector_name, const std::string& region_name, const std::string& property_name) const;
    settings::SettingsNode get_named_property(const std::string& tag_name, const std::string& node_name, const std::string& property_name) const;

  protected:
    Model<ModelVariant>* const model_m;
    const settings::SettingsNode& settings;
    Distance transport_time;
    std::unordered_map<std::string, GeographicPoint> region_centroids;
    FloatType threshold_road_transport = 0;
    FloatType avg_road_speed = 0;
    FloatType avg_water_speed = 0;
    enum class TransportTimeBase { CONST, CENTROIDS, PRECALCULATED };
    TransportTimeBase transport_time_base;

    Sector<ModelVariant>* add_sector(const std::string& name);
    Region<ModelVariant>* add_region(const std::string& name);
    Firm<ModelVariant>* add_firm(Sector<ModelVariant>* sector, Region<ModelVariant>* region);
    Consumer<ModelVariant>* add_consumer(Region<ModelVariant>* region);
    void initialize_connection(Sector<ModelVariant>* sector_from,
                               Region<ModelVariant>* region_from,
                               Sector<ModelVariant>* sector_to,
                               Region<ModelVariant>* region_to,
                               const Flow& flow);
    void initialize_connection(Firm<ModelVariant>* firm_from, EconomicAgent<ModelVariant>* economic_agent_to, const Flow& flow);
    void clean_network();
    void clean_neg_value_added_VA();
    void pre_initialize_variant();
    void post_initialize_variant();
    void read_flows();
    void build_artificial_network();
    void read_transport();
    void read_transport_times_csv(const std::string& index_filename, const std::string& filename);
    void read_centroids_netcdf(const std::string& filename);

  public:
    ModelInitializer(Model<ModelVariant>* model_p, const settings::SettingsNode& settings_p);
    void initialize();
#ifdef DEBUG
    void print_network_characteristics() const;
#endif
    inline Model<ModelVariant>* model() const { return model_m; }
    inline std::string id() const { return "MODELINITIALIZER"; }
};
}  // namespace acclimate

#endif
