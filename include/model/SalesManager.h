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

#ifndef ACCLIMATE_SALESMANAGER_H
#define ACCLIMATE_SALESMANAGER_H

#include <memory>
#include <string>
#include <vector>
#include "types.h"

namespace acclimate {

class BusinessConnection;

class Firm;

class Model;

class SalesManager {
  public:
    Firm* const firm;
    std::vector<std::shared_ptr<BusinessConnection>> business_connections;

  protected:
    Demand sum_demand_requests_D_ = Demand(0.0);
    OpenMPLock sum_demand_requests_D_lock;

  public:
    const Demand& sum_demand_requests_D() const;

  public:
    explicit SalesManager(Firm* firm_p);
    virtual ~SalesManager();
    virtual void distribute(const Flow& production_X) = 0;
    virtual void iterate_expectation();
    void add_demand_request_D(const Demand& demand_request_D);
    void add_initial_demand_request_D_star(const Demand& initial_demand_request_D_star);
    void subtract_initial_demand_request_D_star(const Demand& initial_demand_request_D_star);
    const Flow get_transport_flow() const;
    bool remove_business_connection(BusinessConnection* business_connection);
    Model* model() const;
    std::string id() const;
#ifdef DEBUG
    void print_details() const;
#endif
};
}  // namespace acclimate

#endif
