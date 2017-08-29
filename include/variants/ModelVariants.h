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

#ifndef ACCLIMATE_MODELVARIANTS_H
#define ACCLIMATE_MODELVARIANTS_H

#include "variants/VariantBasic.h"
#include "variants/VariantDemand.h"
#include "variants/VariantPrices.h"

namespace acclimate {

#define INSTANTIATE_BASIC(classname)         \
    template class classname<VariantBasic>;  \
    template class classname<VariantDemand>; \
    template class classname<VariantPrices>

#define INSTANTIATE_DEMAND(classname)        \
    template class classname<VariantDemand>; \
    template class classname<VariantPrices>

#define INSTANTIATE_PRICES(classname) template class classname<VariantPrices>

enum class ModelVariantType { BASIC, DEMAND, PRICES };
}  // namespace acclimate

#endif
