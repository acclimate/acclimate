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

#ifndef ACCLIMATE_PURCHASINGMANAGERDEMAND_H
#define ACCLIMATE_PURCHASINGMANAGERDEMAND_H

#include "model/PurchasingManager.h"

namespace acclimate {

template<class ModelVariant>
class PurchasingManagerDemand : public PurchasingManager<ModelVariant> {
  public:
    using PurchasingManager<ModelVariant>::business_connections;
    using PurchasingManager<ModelVariant>::storage;
    using PurchasingManager<ModelVariant>::get_flow_deficit;

  private:
    using PurchasingManager<ModelVariant>::demand_D_;

  protected:
    void send_demand_requests_D();
    void calc_demand_D();

  public:
    explicit PurchasingManagerDemand(Storage<ModelVariant>* storage_p);
    void iterate_purchase() override;
};
}  // namespace acclimate

#endif
