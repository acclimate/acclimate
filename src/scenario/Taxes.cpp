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

#include "scenario/Taxes.h"
#include <string>
#include "model/Government.h"
#include "run.h"
#include "settingsnode.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
Taxes<ModelVariant>::Taxes(const settings::SettingsNode& settings_p, settings::SettingsNode scenario_node_p, Model<ModelVariant>* const model_p)
    : Scenario<ModelVariant>(settings_p, scenario_node_p, model_p) {}

#ifdef VARIANT_BASIC
template<>
Time Taxes<VariantBasic>::start() {
    error("Taxes scenario not supported in basic model variant");
}
#endif

#ifdef VARIANT_DEMAND
template<>
Time Taxes<VariantDemand>::start() {
    error("Taxes scenario not supported in demand model variant");
}
#endif

template<class ModelVariant>
Time Taxes<ModelVariant>::start() {
    for (auto& region : model()->regions) {
        region->set_government(new Government<ModelVariant>(region.get()));
    }
    return model()->start_time();
}

#ifdef VARIANT_BASIC
template<>
bool Taxes<VariantBasic>::iterate() {
    return false;
}
#endif

#ifdef VARIANT_DEMAND
template<>
bool Taxes<VariantDemand>::iterate() {
    return false;
}
#endif

template<class ModelVariant>
bool Taxes<ModelVariant>::iterate() {
    if (model()->time() > model()->stop_time()) {
        return false;
    }

    for (const settings::SettingsNode& tax : scenario_node["taxes"].as_sequence()) {
        const Time start_tax = tax["start_tax"].as<Time>();
        const Time full_tax = tax["full_tax"].as<Time>();
        if (model()->time() >= start_tax - model()->delta_t() && model()->time() <= full_tax - model()->delta_t()) {
            const Ratio tax_ratio =
                tax["tax_ratio"].as<Ratio>() * (model()->time() + 2 * model()->delta_t() - start_tax) / (full_tax + model()->delta_t() - start_tax);
            const std::string& sector = tax["sector"].as<std::string>();
            if (tax.has("region") && tax["region"].as<std::string>() != "ALL") {
                Region<ModelVariant>* region = model()->find_region(tax["region"].as<std::string>());
                if (!region) {
                    error("Could not find region '" << tax["region"] << "'");
                }
                region->government()->define_tax(sector, tax_ratio);
            } else {
                for (auto& region : model()->regions) {
                    region->government()->define_tax(sector, tax_ratio);
                }
            }
        }
    }

    return true;
}

INSTANTIATE_BASIC(Taxes);
}  // namespace acclimate
