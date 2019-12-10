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

#ifndef ACCLIMATE_PURCHASINGMANAGER_H
#define ACCLIMATE_PURCHASINGMANAGER_H

#include <string>
#include <vector>
#include <memory>
#include "types.h"

namespace acclimate {

class BusinessConnection;
class EconomicAgent;
class Model;
class Storage;

class PurchasingManager {
  protected:
    Demand demand_D_ = Demand(0.0);

  public:
    Storage* const storage;
    std::vector<std::shared_ptr<BusinessConnection>> business_connections;

    const Demand& demand_D(const EconomicAgent* const caller = nullptr) const;
    explicit PurchasingManager(Storage* storage_p);
    virtual ~PurchasingManager();
    virtual const FlowQuantity get_flow_deficit() const;
    virtual const Flow get_transport_flow() const;
    const Flow get_disequilibrium() const;
    FloatType get_stddeviation() const;
    const Flow get_sum_of_last_shipments() const;
    virtual void iterate_consumption_and_production();
    virtual void iterate_purchase() = 0;
    virtual bool remove_business_connection(const BusinessConnection* business_connection);
    const Demand& initial_demand_D_star() const;
    virtual void add_initial_demand_D_star(const Demand& demand_D_p);
    virtual void subtract_initial_demand_D_star(const Demand& demand_D_p);
    Model* model() const;
    std::string id() const;
#ifdef DEBUG
    void print_details() const;
#endif
};
}  // namespace acclimate

#endif
