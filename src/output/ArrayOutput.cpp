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

#include "output/ArrayOutput.h"

#include <limits>
#include <utility>

#include "ModelRun.h"
#include "model/Model.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "settingsnode.h"

namespace acclimate {

ArrayOutput::ArrayOutput(const settings::SettingsNode& settings_p, Model* model_p, settings::SettingsNode output_node_p, bool over_time_p)
    : Output(settings_p, model_p, std::move(output_node_p)), over_time(over_time_p) {
    include_events = false;
}

void ArrayOutput::initialize() {
    include_events = output_node["events"].template as<bool>(false);
    sectors_size = model()->sectors.size();
    regions_size = model()->regions.size();
}

inline typename ArrayOutput::Variable& ArrayOutput::create_variable(const hstring& path, const hstring& name, const hstring& suffix) {
    const auto key = suffix ^ name ^ path;
    auto it = variables.find(key);
    if (it == variables.end()) {
        std::size_t size = 1;
        std::vector<std::size_t> shape;
        for (const auto& t : stack) {
            if (t.sector != nullptr) {
                shape.push_back(sectors_size);
                size *= sectors_size;
            }
            if (t.region != nullptr) {
                shape.push_back(regions_size);
                size *= regions_size;
            }
        }
        Variable v{std::vector<FloatType>(size, std::numeric_limits<FloatType>::quiet_NaN()), shape, size, nullptr};
        create_variable_meta(v, path, name, suffix);
        return variables.emplace(key, v).first->second;
    }
    return it->second;
}

void ArrayOutput::internal_write_value(const hstring& name, FloatType v, const hstring& suffix) {
    const Target& t = stack.back();
    Variable& it = create_variable(t.name, name, suffix);
    it.data[t.index] = v;
}

inline std::size_t ArrayOutput::current_index() const {
    if (!stack.empty()) {
        return stack.back().index;
    }
    if (over_time) {
        return model()->timestep();
    }
    return 0;
}

void ArrayOutput::internal_start_target(const hstring& name, Sector* sector, Region* region) {
    stack.emplace_back(Target{name, current_index() * regions_size * sectors_size + sector->index() * regions_size + region->index(), sector, region});
}

void ArrayOutput::internal_start_target(const hstring& name, Sector* sector) {
    stack.emplace_back(Target{name, current_index() * sectors_size + sector->index(), sector, nullptr});
}

void ArrayOutput::internal_start_target(const hstring& name, Region* region) {
    stack.emplace_back(Target{name, current_index() * regions_size + region->index(), nullptr, region});
}

void ArrayOutput::internal_start_target(const hstring& name) { stack.emplace_back(Target{name, current_index(), nullptr, nullptr}); }

void ArrayOutput::internal_end_target() { stack.pop_back(); }

void ArrayOutput::internal_iterate_begin() {
    if (over_time) {
        const unsigned int t = model()->timestep();
        for (auto& var : variables) {
            Variable& v = var.second;
            v.data.resize((t + 1) * v.size, std::numeric_limits<FloatType>::quiet_NaN());
        }
    }
}

const typename ArrayOutput::Variable& ArrayOutput::get_variable(const hstring& fullname) const {
    const auto it = variables.find(fullname);
    if (it == variables.end()) {
        throw log::error(this, "Variable '", fullname, "' not found");
    }
    return it->second;
}

void ArrayOutput::event(
    EventType type, const Sector* sector_from, const Region* region_from, const Sector* sector_to, const Region* region_to, FloatType value) {
    if (include_events) {
        Event event_struct;
        event_struct.time = model()->timestep();
        event_struct.type = static_cast<unsigned char>(type);
        event_struct.sector_from = (sector_from == nullptr ? -1 : sector_from->index());
        event_struct.region_from = (region_from == nullptr ? -1 : region_from->index());
        event_struct.sector_to = (sector_to == nullptr ? -1 : sector_to->index());
        event_struct.region_to = (region_to == nullptr ? -1 : region_to->index());
        event_struct.value = value;
        if (internal_handle_event(event_struct)) {
            events.emplace_back(event_struct);
        }
    }
}

}  // namespace acclimate
