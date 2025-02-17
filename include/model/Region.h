// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_REGION_H
#define ACCLIMATE_REGION_H

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <utility>

#include "acclimate.h"
#include "model/GeoLocation.h"
#include "model/GeoRoute.h"  // IWYU pragma: keep
#include "model/Sector.h"
#include "openmp.h"

namespace acclimate {

class EconomicAgent;
class Government;
class Model;

class Region final : public GeoLocation {
    friend class ModelInitializer;

  private:
    struct route_hash {
        std::size_t operator()(const std::pair<IndexType, Sector::transport_type_t>& p) const { return (p.first << 3) | (static_cast<IntType>(p.second)); }
    };

  private:
    Flow export_flow_[2] = {Flow(0.0), Flow(0.0)};
    openmp::Lock export_flow_lock_;
    Flow import_flow_[2] = {Flow(0.0), Flow(0.0)};
    openmp::Lock import_flow_lock_;
    Flow consumption_flow_Y_[2] = {Flow(0.0), Flow(0.0)};
    openmp::Lock consumption_flow_Y_lock_;
    std::unordered_map<std::pair<IndexType, Sector::transport_type_t>, GeoRoute, route_hash> routes_;  // TODO improve
    std::unique_ptr<Government> government_;
    openmp::Lock economic_agents_lock_;

  public:
    non_owning_vector<EconomicAgent> economic_agents;

  public:
    Region(Model* model, id_t id);
    ~Region() override;
    const Flow& consumption() const;
    const Flow& import_flow() const;
    const Flow& export_flow() const;
    void set_government(Government* government_p);
    Government* government();
    const Government* government() const;
    void add_export(const Flow& flow);
    void add_import(const Flow& flow);
    void add_consumption(const Flow& flow);
    Flow get_gdp() const;
    void iterate_consumption_and_production();
    void iterate_expectation();
    void iterate_purchase();
    void iterate_investment();
    GeoRoute& find_path_to(Region* region, Sector::transport_type_t transport_type);
    Region* as_region() override { return this; }
    const Region* as_region() const override { return this; }

    template<typename Observer, typename H>
    bool observe(Observer& o) const {
        return GeoLocation::observe<Observer, H>(o)  //
               && o.set(H::hash("import"),
                        [this]() {  //
                            return import_flow();
                        })
               && o.set(H::hash("export"),
                        [this]() {  //
                            return export_flow();
                        })
               && o.set(H::hash("consumption"),
                        [this]() {  //
                            return consumption();
                        })
               && o.set(H::hash("gdp"),
                        [this]() {  //
                            return get_gdp();
                        })
            //
            ;
    }
};
}  // namespace acclimate

#endif
