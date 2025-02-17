// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <cstring>

#include "acclimate.h"
#include "model/Model.h"
#include "output/ArrayOutput.h"
#include "version.h"

namespace acclimate {

static void acclimate_get_variable(const char* name, const FloatType** data, std::size_t* size, const std::size_t** shape, std::size_t* dimension) {
    const ArrayOutput::Variable& var = static_cast<const ArrayOutput*>(Acclimate::Run::instance()->output(0))->get_variable(name);
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
        const ArrayOutput::Event& e = output->get_events()[index];
        *timestep = e.time;
        std::string desc = std::string(Acclimate::event_names[e.type]) + " " + (e.sector_from < 0 ? "" : output->model->sectors_C[e.sector_from]->name())
                           + (e.sector_from >= 0 && e.region_from >= 0 ? ":" : "") + (e.region_from < 0 ? "" : output->model->regions_R[e.region_from]->name())
                           + (e.sector_to < 0 ? "" : output->model->sectors_C[e.sector_to]->name()) + (e.sector_to >= 0 && e.region_to >= 0 ? ":" : "")
                           + (e.region_to < 0 ? "" : output->model->regions_R[e.region_to]->name());
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
