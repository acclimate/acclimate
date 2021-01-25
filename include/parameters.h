/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
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

#ifndef PARAMETERS_H
#define PARAMETERS_H

#include "types.h"

namespace acclimate {

// TODO get rid of Parameters struct

struct Parameters {
    struct AgentParameters {};

    struct ModelParameters {
        Price cheapest_price_range_width = Price(0.0);
        Price transport_penalty_large = Price(0.0);
        Price transport_penalty_small = Price(0.0);
        Ratio min_storage = Ratio(0.0);
        bool always_extend_expected_demand_curve;  // extend incoming demand in expectation step with certain elasticity
        bool cheapest_price_range_generic_size;
        bool cheapest_price_range_preserve_seller_price;
        bool deviation_penalty;
        bool maximal_decrease_reservation_price_limited_by_markup;
        bool naive_expectations;           // incoming demand is never extended
        bool quadratic_transport_penalty;  // quadratic instead of linear transport penalty
        bool relative_transport_penalty;
        bool respect_markup_in_production_extension;
        int optimization_algorithm;
        int optimization_maxiter;           // maximal iteration
        unsigned int optimization_timeout;  // timeout in sec
    };

    struct RegionParameters {};

    struct SectorParameters {
        Price estimated_price_increase_production_extension = Price(0.0);
        Price initial_markup = Price(0.0);
        Price price_increase_production_extension = Price(0.0);
        Ratio supply_elasticity = Ratio(0.0);
        Time target_storage_refill_time = Time(0.0);
        Time target_storage_withdraw_time = Time(0.0);
    };

    struct StorageParameters {
        Ratio consumption_price_elasticity = Ratio(0.0);
    };
};
}  // namespace acclimate

#endif
