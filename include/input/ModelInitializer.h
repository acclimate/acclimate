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

#include "settingsnode.h"
#include "types.h"

namespace mrio {

template<typename ValueType, typename IndexType>
class Table;

}  // namespace mrio

namespace acclimate {

class Consumer;
class EconomicAgent;
class GeoEntity;
class Model;
class Firm;
class Region;
class Sector;

class ModelInitializer {
  protected:
    class TemporaryGeoEntity {
      protected:
        std::unique_ptr<GeoEntity> entity_m;

      public:
        bool used;

        GeoEntity* entity() { return entity_m.get(); }

        TemporaryGeoEntity(GeoEntity* entity_p, bool used_p) : entity_m(entity_p), used(used_p) {}

        ~TemporaryGeoEntity() {
            if (used) {
                (void)entity_m.release();
            }
        }
    };

    class Path {
      protected:
        FloatType costs_m = 0;
        std::vector<TemporaryGeoEntity*> points_m;

      public:
        Path() = default;

        Path(FloatType costs_p, TemporaryGeoEntity* p1, TemporaryGeoEntity* p2, TemporaryGeoEntity* connection)
            : costs_m(costs_p), points_m({p1, connection, p2}) {}

        inline FloatType costs() const { return costs_m; }

        inline bool empty() const { return points_m.empty(); }

        inline const std::vector<TemporaryGeoEntity*>& points() const { return points_m; }

        Path operator+(const Path& other) const {
            Path res;
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

  private:
    settings::SettingsNode get_firm_property(const std::string& sector_name, const std::string& region_name, const std::string& property_name) const;
    settings::SettingsNode get_named_property(const settings::SettingsNode& node_settings,
                                              const std::string& node_name,
                                              const std::string& property_name) const;

  protected:
    Model* const model_m;
    const settings::SettingsNode& settings;

    Sector* add_sector(const std::string& name);
    Region* add_region(const std::string& name);
    Firm* add_firm(Sector* sector, Region* region);
    Consumer* add_consumer(Region* region);
    void create_simple_transport_connection(Region* region_from, Region* region_to, TransportDelay transport_delay);
    void initialize_connection(Sector* sector_from, Region* region_from, Sector* sector_to, Region* region_to, const Flow& flow);
    void initialize_connection(Firm* firm_from, EconomicAgent* economic_agent_to, const Flow& flow);
    void clean_network();
    void pre_initialize();
    void post_initialize();
    void build_agent_network();
    void build_agent_network_from_table(const mrio::Table<FloatType, std::size_t>& table, FloatType flow_threshold);
    void build_artificial_network();
    void build_transport_network();
    void read_transport_times_csv(const std::string& index_filename, const std::string& filename);
    void read_centroids_netcdf(const std::string& filename);
    void read_transport_network_netcdf(const std::string& filename);

  public:
    ModelInitializer(Model* model_p, const settings::SettingsNode& settings_p);
    void initialize();
    void print_network_characteristics() const;

    inline Model* model() const { return model_m; }

    inline std::string id() const { return "MODELINITIALIZER"; }
};
}  // namespace acclimate

#endif
