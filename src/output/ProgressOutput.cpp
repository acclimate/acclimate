/*
  Copyright (C) 2014-2018 Sven Willner <sven.willner@pik-potsdam.de>
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

#include "output/ProgressOutput.h"
#include <iostream>
#include "model/Model.h"
#include "model/Sector.h"
#include "scenario/Scenario.h"
#include "variants/ModelVariants.h"
#ifdef USE_TQDM
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#include "tqdm/tqdm.h"
#pragma GCC diagnostic pop
#endif

namespace acclimate {

template<class ModelVariant>
ProgressOutput<ModelVariant>::ProgressOutput(const settings::SettingsNode& settings_p,
                                             Model<ModelVariant>* model_p,
                                             Scenario<ModelVariant>* scenario_p,
                                             settings::SettingsNode output_node_p)
    : Output<ModelVariant>(settings_p, model_p, scenario_p, std::move(output_node_p)) {}

template<class ModelVariant>
void ProgressOutput<ModelVariant>::initialize() {
#ifdef USE_TQDM
    tqdm::Params p;
    p.ascii = "";
    p.f = stdout;
    total = output_node["total"].template as<int>();
    it.reset(new tqdm::RangeTqdm<int>(tqdm::RangeIterator<int>(total), tqdm::RangeIterator<int>(total, total), p));
#else
    error("tqdm not enabled");
#endif
}

template<class ModelVariant>
void ProgressOutput<ModelVariant>::checkpoint_stop() {
#ifdef USE_TQDM
    total = it->size_remaining();
    it->close();
#endif
}

template<class ModelVariant>
void ProgressOutput<ModelVariant>::checkpoint_resume() {
#ifdef USE_TQDM
    tqdm::Params p;
    p.ascii = "";
    p.f = stdout;
    p.miniters = 1;
    it.reset(new tqdm::RangeTqdm<int>(tqdm::RangeIterator<int>(total), tqdm::RangeIterator<int>(total, total), p));
#endif
}

template<class ModelVariant>
void ProgressOutput<ModelVariant>::internal_end() {
#ifdef USE_TQDM
    it->close();
#endif
}

template<class ModelVariant>
void ProgressOutput<ModelVariant>::internal_iterate_end() {
#ifdef USE_TQDM
    if (!it->ended()) {
        ++(*it);
        std::cout << std::flush;
    }
#endif
}

INSTANTIATE_BASIC(ProgressOutput);
}  // namespace acclimate
