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

#ifndef ACCLIMATE_PURCHASINGMANAGERBASIC_H
#define ACCLIMATE_PURCHASINGMANAGERBASIC_H

#include "model/PurchasingManager.h"

namespace acclimate {

template<class ModelVariant>
class PurchasingManagerBasic : public PurchasingManager<ModelVariant> {
  public:
    using PurchasingManager<ModelVariant>::business_connections;

  public:
    explicit PurchasingManagerBasic(Storage<ModelVariant>* storage_p);
    void iterate_purchase() override;
};
}  // namespace acclimate

#endif
