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

#ifndef ACCLIMATE_ARRAYOUTPUT_H
#define ACCLIMATE_ARRAYOUTPUT_H

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "output/Output.h"

namespace settings {
class SettingsNode;
}  // namespace settings

namespace acclimate {

class Model;
class Region;
class Sector;
class Scenario;

class ArrayOutput : public Output {
  public:
    using Output::id;
    using Output::model;
    using Output::output_node;
    using Output::scenario;

    struct Variable {
        std::vector<FloatType> data;
        std::vector<std::size_t> shape;  // without time
        std::size_t size = 0;            // without time
        void* meta = nullptr;

        Variable(std::vector<FloatType> data_p, std::vector<std::size_t> shape_p, std::size_t size_p, void* meta_p)
            : data(std::move(data_p)), shape(std::move(shape_p)), size(size_p), meta(meta_p) {}
    };

    struct Event {
        std::size_t time = 0;
        unsigned char type = 0;
        int sector_from = 0;
        int region_from = 0;
        int sector_to = 0;
        int region_to = 0;
        FloatType value = 0;
    };

  protected:
    struct Target {
        hstring name;
        std::size_t index = 0;
        Sector* sector;
        Region* region;

        Target(hstring name_p, std::size_t index_p, Sector* sector_p, Region* region_p)
            : name(std::move(name_p)), index(index_p), sector(sector_p), region(region_p) {}
    };

    std::size_t sectors_size = 0;
    std::size_t regions_size = 0;
    std::unordered_map<hstring::hash_type, Variable> variables;
    std::vector<Target> stack;
    std::vector<Event> events;
    bool include_events;
    bool over_time;

  protected:
    void internal_write_value(const hstring& name, FloatType v, const hstring& suffix) override;
    void internal_start_target(const hstring& name, Sector* sector, Region* region) override;
    void internal_start_target(const hstring& name, Sector* sector) override;
    void internal_start_target(const hstring& name, Region* region) override;
    void internal_start_target(const hstring& name) override;
    void internal_end_target() override;
    void internal_iterate_begin() override;
    inline std::size_t current_index() const;
    inline Variable& create_variable(const hstring& path, const hstring& name, const hstring& suffix);

    virtual void create_variable_meta(Variable& v, const hstring& path, const hstring& name, const hstring& suffix) {
        UNUSED(v);
        UNUSED(path);
        UNUSED(name);
        UNUSED(suffix);
    }

    virtual bool internal_handle_event(Event& event) {
        UNUSED(event);
        return true;
    }

  public:
    ArrayOutput(const settings::SettingsNode& settings_p, Model* model_p, Scenario* scenario_p, settings::SettingsNode output_node_p, bool over_time_p = true);
    ~ArrayOutput() override = default;
    void event(
        EventType type, const Sector* sector_from, const Region* region_from, const Sector* sector_to, const Region* region_to, FloatType value) override;
    void initialize() override;
    const typename ArrayOutput::Variable& get_variable(const hstring& fullname) const;

    const std::vector<Event>& get_events() const { return events; }
};
}  // namespace acclimate

#endif
