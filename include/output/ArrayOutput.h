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

#include <unordered_map>
#include "output/Output.h"

namespace acclimate {

template<class ModelVariant>
class Model;
template<class ModelVariant>
class Region;
template<class ModelVariant>
class Sector;
template<class ModelVariant>
class Scenario;

template<class ModelVariant>
class ArrayOutput : public Output<ModelVariant> {
  public:
    using Output<ModelVariant>::output_node;
    using Output<ModelVariant>::model;
    using Output<ModelVariant>::settings;
    using Output<ModelVariant>::scenario;
    struct Variable {
        std::vector<FloatType> data;
        std::vector<std::size_t> shape;  // without time
        std::size_t size;                // without time
        void* meta;
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
        std::string name;
        std::size_t index;
        Sector<ModelVariant>* sector;
        Region<ModelVariant>* region;
    };
    std::size_t sectors_size = 0;
    std::size_t regions_size = 0;
    std::unordered_map<std::string, Variable> variables;
    std::vector<Target> stack;
    std::vector<Event> events;
    bool include_events;
    bool over_time;

  protected:
    void internal_write_value(const std::string& name, const FloatType& v) override;
    void internal_start_target(const std::string& name, Sector<ModelVariant>* sector, Region<ModelVariant>* region) override;
    void internal_start_target(const std::string& name, Sector<ModelVariant>* sector) override;
    void internal_start_target(const std::string& name, Region<ModelVariant>* region) override;
    void internal_start_target(const std::string& name) override;
    void internal_end_target() override;
    virtual void internal_iterate_begin() override;
    inline std::size_t current_index() const;
    inline Variable& create_variable(const std::string& path, const std::string& name);
    virtual void create_variable_meta(Variable& v, const std::string& path, const std::string& name) {
        UNUSED(v);
        UNUSED(path);
        UNUSED(name);
    };
    virtual bool internal_handle_event(Event& event) {
        UNUSED(event);
        return true;
    };

  public:
    ArrayOutput(const settings::SettingsNode& settings_p,
                Model<ModelVariant>* model_p,
                Scenario<ModelVariant>* scenario_p,
                const settings::SettingsNode& output_node_p,
                const bool over_time_p = true);
    virtual ~ArrayOutput(){};
    void event(const EventType& type,
               const Sector<ModelVariant>* sector_from,
               const Region<ModelVariant>* region_from,
               const Sector<ModelVariant>* sector_to,
               const Region<ModelVariant>* region_to,
               const FloatType& value) override;
    virtual void initialize() override;
    const typename ArrayOutput<ModelVariant>::Variable& get_variable(const std::string& fullname) const;
    const std::vector<Event>& get_events() const { return events; };
};
}  // namespace acclimate

#endif
