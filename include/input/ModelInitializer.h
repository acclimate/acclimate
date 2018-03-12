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

#include <unordered_map>
#include "settingsnode.h"
#include "variants/VariantPrices.h"

namespace mrio {
template<typename ValueType, typename IndexType>
class Table;
}

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
class GeographicPoint;

template<class ModelVariant>
class ModelInitializer {
  protected:
    class TemporaryGeoEntity {
      public:
        bool used;
        GeoEntity<ModelVariant>* entity;
        TemporaryGeoEntity(GeoEntity<ModelVariant>* entity_p, bool used_p) : used(used_p), entity(entity_p) {}
        ~TemporaryGeoEntity() {
            if (!used) {
                delete entity;
            }
        }
    };
    class Path {
      protected:
        FloatType costs_m;
        std::vector<TemporaryGeoEntity*> points_m;

      public:
        Path() : costs_m(0) {}
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
    settings::SettingsNode get_named_property(const std::string& tag_name, const std::string& node_name, const std::string& property_name) const;

  protected:
    Model<ModelVariant>* const model;
    const settings::SettingsNode& settings;

    Sector<ModelVariant>* add_sector(const std::string& name);
    Region<ModelVariant>* add_region(const std::string& name);
    Firm<ModelVariant>* add_firm(Sector<ModelVariant>* sector, Region<ModelVariant>* region);
    Consumer<ModelVariant>* add_consumer(Region<ModelVariant>* region);
    void create_simple_transport_connection(Region<ModelVariant>* region_from, Region<ModelVariant>* region_to, TransportDelay transport_delay);
    void initialize_connection(Sector<ModelVariant>* sector_from,
                               Region<ModelVariant>* region_from,
                               Sector<ModelVariant>* sector_to,
                               Region<ModelVariant>* region_to,
                               const Flow& flow);
    void initialize_connection(Firm<ModelVariant>* firm_from, EconomicAgent<ModelVariant>* economic_agent_to, const Flow& flow);
    void clean_network();
    void pre_initialize_variant();
    void post_initialize_variant();
    void build_agent_network();
    void build_agent_network_from_table(const mrio::Table<FloatType, std::size_t>& table, FloatType flow_threshold);
    void build_artificial_network();
    void build_transport_network();
    void read_transport_times_csv(const std::string& index_filename, const std::string& filename);
    void read_centroids_netcdf(const std::string& filename);
    void read_transport_network_netcdf(const std::string& filename);

  public:
    ModelInitializer(Model<ModelVariant>* model_p, const settings::SettingsNode& settings_p);
    void initialize();
#ifdef DEBUG
    void print_network_characteristics() const;
    inline operator std::string() const { return "MODELINITIALIZER"; }
#endif
};
}  // namespace acclimate

#endif
