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

#include "settingsnode/pybind.h"
#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include "acclimate.h"
#include "model/Model.h"
#include "output/ArrayOutput.h"
#include "settingsnode.h"
#include "variants/VariantPrices.h"
#include "version.h"

namespace py = pybind11;

#ifdef ACCLIMATE_HAS_DIFF
extern const char* acclimate_git_diff;
#endif
extern const char* acclimate_info;

namespace acclimate {

PYBIND11_MODULE(model, m) {
    m.doc() = R"pbdoc(
        Acclimate model
        ---------------
        Authors: Sven Willner <sven.willner@pik-potsdam.de>
                 Christian Otto <christian.otto@pik-potsdam.de>
    )pbdoc";

    m.def("run", [](py::dict settings) {
        // try {
            acclimate::Acclimate::initialize(settings::SettingsNode(std::unique_ptr<settings::PyNode>(new settings::PyNode(settings))));
            const int res = acclimate::Acclimate::instance()->run();
            if (res == 0) {
                acclimate::Acclimate::instance()->cleanup();
            } else {
                acclimate::Acclimate::instance()->memory_cleanup();
            }
        // } catch (std::runtime_error& ex) {
        //     acclimate::Acclimate::instance()->memory_cleanup();
        //     throw ex;
        // }
    });

    m.attr("__version__") = ACCLIMATE_VERSION;
    m.attr("__info__") = acclimate_info;
#ifdef ACCLIMATE_HAS_DIFF
    m.attr("__diff__") = acclimate_git_diff;
#endif
}

// template<class ModelVariant>
// static void acclimate_get_variable(const char* name, const FloatType** data, std::size_t* size, const std::size_t** shape, std::size_t* dimension) {
//     const typename ArrayOutput<ModelVariant>::Variable& var =
//         static_cast<const ArrayOutput<ModelVariant>*>(Acclimate::Run<ModelVariant>::instance()->output(0))->get_variable(name);
//     *data = &var.data[0];
//     *size = var.data.size();
//     *shape = &var.shape[0];
//     *dimension = var.shape.size();
// }

// template<class ModelVariant>
// static void acclimate_get_event(const std::size_t index, std::size_t* timestep, char* event, FloatType* value) {
//     const ArrayOutput<ModelVariant>* output = static_cast<const ArrayOutput<ModelVariant>*>(Acclimate::Run<ModelVariant>::instance()->output(0));
//     if (index >= output->get_events().size()) {
//         *timestep = 0;
//         event[0] = '\0';
//         *value = std::numeric_limits<FloatType>::quiet_NaN();
//     } else {
//         const typename ArrayOutput<ModelVariant>::Event& e = output->get_events()[index];
//         *timestep = e.time;
//         std::string desc = std::string(Acclimate::event_names[e.type]) + " " + (e.sector_from < 0 ? "" : output->model->sectors_C[e.sector_from]->id())
//                            + (e.sector_from >= 0 && e.region_from >= 0 ? ":" : "") + (e.region_from < 0 ? "" : output->model->regions_R[e.region_from]->id())
//                            + (e.sector_to < 0 ? "" : output->model->sectors_C[e.sector_to]->id()) + (e.sector_to >= 0 && e.region_to >= 0 ? ":" : "")
//                            + (e.region_to < 0 ? "" : output->model->regions_R[e.region_to]->id());
//         std::memcpy(event, desc.c_str(), desc.length() + 1);
//         *value = std::numeric_limits<FloatType>::quiet_NaN();
//     }
// }

}  // namespace acclimate
