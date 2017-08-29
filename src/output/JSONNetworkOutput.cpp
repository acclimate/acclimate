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

#include "output/JSONNetworkOutput.h"
#include <algorithm>
#include <fstream>
#include "model/BusinessConnection.h"
#include "model/CapacityManager.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/Model.h"
#include "model/PurchasingManager.h"
#include "model/Region.h"
#include "model/SalesManager.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "scenario/Scenario.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
JSONNetworkOutput<ModelVariant>::JSONNetworkOutput(const settings::SettingsNode& settings_p,
                                                   Model<ModelVariant>* model_p,
                                                   Scenario<ModelVariant>* scenario_p,
                                                   const settings::SettingsNode& output_node_p)
    : Output<ModelVariant>(settings_p, model_p, scenario_p, output_node_p) {}

template<class ModelVariant>
void JSONNetworkOutput<ModelVariant>::initialize() {
    timestep = output_node["timestep"].template as<TimeStep>();
    filename = output_node["file"].template as<std::string>();
    advanced = output_node["advanced"].template as<bool>();
}

template<class ModelVariant>
void JSONNetworkOutput<ModelVariant>::internal_iterate_end() {
    if (model->timestep() == timestep) {
        std::ofstream out;
        out.open(filename.c_str(), std::ofstream::out);

        out << "\"nodes\": [";
        for (const auto& region : model->regions_R) {
            for (const auto& ea : region->economic_agents) {
                out << "\n  {";
                std::string sector;
                Flow out_flow = Flow(0.0);
                Flow in_flow = Flow(0.0);
                Flow used_flow = Flow(0.0);
                if (ea->type == EconomicAgent<ModelVariant>::Type::FIRM) {
                    Firm<ModelVariant>* ps = ea->as_firm();
                    sector = std::string(*ps->sector);
                    out_flow = ps->production_X();
                    if (advanced) {
                        out << "\n    \"production_capacity\": " << (ps->production_X() / ps->initial_production_X_star()) << ",";
                    }
                } else {
                    sector = "FCON";
                    out_flow = Flow(0.0);
                }
                if (advanced) {
                    for (const auto& is : ea->input_storages) {
                        in_flow += is->last_input_flow_I();
                        used_flow += is->used_flow_U();
                    }
                }
                out << "\n    \"sector\": \"" << sector << "\",";
                out << "\n    \"region\": \"" << std::string(*ea->region) << "\"";
                if (advanced) {
                    out << ",\n    \"in_flow\": " << in_flow.get_quantity() << ",";
                    out << "\n    \"used_flow\": " << used_flow.get_quantity() << ",";
                    out << "\n    \"out_flow\": " << out_flow.get_quantity();
                }
                out << "\n  },";
            }
        }
        out.seekp(-1, std::ios_base::end);
        out << "\n],\n";

        out << "\"links\": [ ";
        for (const auto& region : model->regions_R) {
            for (const auto& ea : region->economic_agents) {
                for (const auto& is : ea->input_storages) {
                    for (const auto& bc : is->purchasing_manager->business_connections) {
                        out << "\n  {";
                        out << "\n    \"source\": \"" << std::string(*bc->seller->firm) << "\",";
                        out << "\n    \"target\": \"" << std::string(*ea) << "\",";
                        out << "\n    \"distance\": " << static_cast<int>(bc->get_transport_delay_tau()) << ",";
                        out << "\n    \"flow\": " << bc->last_shipment_Z().get_quantity();
                        out << "\n  },";
                    }
                }
            }
        }
        out.seekp(-1, std::ios_base::end);
        out << "\n  ]\n";
        out << "}\n";
        out.close();
    }
}

INSTANTIATE_BASIC(JSONNetworkOutput);
}  // namespace acclimate
