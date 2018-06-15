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

#include "model/EconomicAgent.h"

namespace acclimate {

template<class ModelVariant>
class BusinessConnection;
template<class ModelVariant>
class Storage;

template<class ModelVariant>
class PurchasingManager {
  public:
    Storage<ModelVariant>* const storage;
    std::vector<BusinessConnection<ModelVariant>*> business_connections;

  protected:
    Demand demand_D_ = Demand(0.0);

  public:
    inline const Demand& demand_D(const EconomicAgent<ModelVariant>* const caller = nullptr) const {
#ifdef DEBUG
        if (caller != storage->economic_agent) {
            assertstepnot(PURCHASE);
        }
#else
        UNUSED(caller);
#endif
        return demand_D_;
    };

  protected:
  public:
    explicit PurchasingManager(Storage<ModelVariant>* storage_p);
    virtual ~PurchasingManager(){};
    virtual const FlowQuantity get_flow_deficit() const;
    virtual const Flow get_transport_flow() const;
    const Flow get_disequilibrium() const;
    FloatType get_stddeviation() const;
    const Flow get_sum_of_last_shipments() const;
    virtual void iterate_consumption_and_production();
    virtual void iterate_purchase() = 0;
    virtual bool remove_business_connection(const BusinessConnection<ModelVariant>* business_connection);
    inline operator std::string() const { return std::string(*storage->sector) + "->" + std::string(*storage->economic_agent); }
    inline const Demand& initial_demand_D_star() const { return storage->initial_input_flow_I_star(); }
    virtual void add_initial_demand_D_star(const Demand& demand_D_p);
    virtual void subtract_initial_demand_D_star(const Demand& demand_D_p);
#ifdef DEBUG
    void print_details() const;
#endif
};
}  // namespace acclimate

#endif
