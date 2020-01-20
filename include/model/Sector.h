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

#ifndef ACCLIMATE_SECTOR_H
#define ACCLIMATE_SECTOR_H

#include <string>
#include <vector>

#include "openmp.h"
#include "parameters.h"
#include "settingsnode.h"
#include "types.h"

namespace acclimate {

class Model;
class Firm;

class Sector {
    friend class Model;

  public:
    enum class TransportType { AVIATION, IMMEDIATE, ROADSEA };

  private:
    const IndexType index_m;
    const std::string id_m;
    Demand total_demand_D_ = Demand(0.0);
    OpenMPLock total_demand_D_lock;
    Flow total_production_X_m = Flow(0.0);
    OpenMPLock total_production_X_lock;
    Flow last_total_production_X_m = Flow(0.0);
    Parameters::SectorParameters parameters_m;
    Model* const model_m;

  public:
    const Ratio upper_storage_limit_omega;
    const Time initial_storage_fill_factor_psi;
    const TransportType transport_type;
    std::vector<Firm*> firms;

  private:
    Sector(Model* model_p,
           std::string id_p,
           IndexType index_p,
           Ratio upper_storage_limit_omega_p,
           Time initial_storage_fill_factor_psi_p,
           TransportType transport_type_p);

  public:
    static TransportType map_transport_type(const settings::hstring& transport_type);
    static const char* unmap_transport_type(TransportType transport_type);
    const Demand& total_demand_D() const;
    const Demand& total_production_X() const;
    const Parameters::SectorParameters& parameters() const { return parameters_m; }
    Parameters::SectorParameters& parameters_writable();
    void add_demand_request_D(const Demand& demand_request_D);
    void add_production_X(const Flow& production_X);
    void add_initial_production_X(const Flow& production_X);
    void subtract_initial_production_X(const Flow& production_X);
    void iterate_consumption_and_production();
    void remove_firm(Firm* firm);

    IndexType index() const { return index_m; }

    Model* model() const { return model_m; }
    const std::string& id() const { return id_m; }
};
}  // namespace acclimate

#endif
