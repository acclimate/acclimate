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

#include <cstring>

#include "acclimate.h"
#include "model/Model.h"
#include "output/ArrayOutput.h"
#include "version.h"

namespace acclimate {

static void acclimate_get_variable(const char* name, const FloatType** data, std::size_t* size, const std::size_t** shape, std::size_t* dimension) {
    const typename ArrayOutput::Variable& var = static_cast<const ArrayOutput*>(Acclimate::Run::instance()->output(0))->get_variable(name);
    *data = &var.data[0];
    *size = var.data.size();
    *shape = &var.shape[0];
    *dimension = var.shape.size();
}

static void acclimate_get_event(const std::size_t index, std::size_t* timestep, char* event, FloatType* value) {
    const ArrayOutput* output = static_cast<const ArrayOutput*>(Acclimate::Run::instance()->output(0));
    if (index >= output->get_events().size()) {
        *timestep = 0;
        event[0] = '\0';
        *value = std::numeric_limits<FloatType>::quiet_NaN();
    } else {
        const typename ArrayOutput::Event& e = output->get_events()[index];
        *timestep = e.time;
        std::string desc = std::string(Acclimate::event_names[e.type]) + " " + (e.sector_from < 0 ? "" : output->model->sectors_C[e.sector_from]->id())
                           + (e.sector_from >= 0 && e.region_from >= 0 ? ":" : "") + (e.region_from < 0 ? "" : output->model->regions_R[e.region_from]->id())
                           + (e.sector_to < 0 ? "" : output->model->sectors_C[e.sector_to]->id()) + (e.sector_to >= 0 && e.region_to >= 0 ? ":" : "")
                           + (e.region_to < 0 ? "" : output->model->regions_R[e.region_to]->id());
        std::memcpy(event, desc.c_str(), desc.length() + 1);
        *value = std::numeric_limits<FloatType>::quiet_NaN();
    }
}

extern "C" {

extern const char* git_diff;
static std::string last_error;

const char* acclimate_get_last_error() { return last_error.c_str(); }
const char* acclimate_get_version() { return ACCLIMATE_VERSION; }
bool acclimate_get_openmp() {
#ifdef _OPENMP
    return true;
#else
    return false;
#endif
}
const char* acclimate_get_options_string() { return ACCLIMATE_OPTIONS; }

int acclimate_initialize(const char* settings) {
    try {
        std::istringstream ss(settings);
        Acclimate::initialize(settings::SettingsNode(ss));
        return 0;
    } catch (const std::exception& ex) {
        last_error = ex.what();
        return -1;
    }
}

int acclimate_run() {
    try {
        Acclimate::instance()->run();
        return 0;
    } catch (const std::exception& ex) {
        last_error = ex.what();
        return -1;
    }
}

int acclimate_get_variable_prices(const char* name, const FloatType** data, std::size_t* size, const std::size_t** shape, std::size_t* dimension) {
    try {
        acclimate_get_variable(name, data, size, shape, dimension);
        return 0;
    } catch (const std::exception& ex) {
        last_error = ex.what();
        return -1;
    }
}

int acclimate_get_event_prices(const std::size_t index, std::size_t* timestep, char* event, FloatType* value) {
    try {
        acclimate_get_event(index, timestep, event, value);
        return 0;
    } catch (const std::exception& ex) {
        last_error = ex.what();
        return -1;
    }
}

int acclimate_cleanup() {
    try {
        Acclimate::instance()->cleanup();
        return 0;
    } catch (const std::exception& ex) {
        last_error = ex.what();
        return -1;
    }
}
}
}  // namespace acclimate
