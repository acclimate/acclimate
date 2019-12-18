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

#ifndef ACCLIMATE_OUTPUT_H
#define ACCLIMATE_OUTPUT_H

#include <ctime>
#include <string>

#include "run.h"
#include "settingsnode.h"
#include "types.h"

namespace acclimate {

using hstring = settings::hstring;

class Scenario;

class BusinessConnection;

class Consumer;

class EconomicAgent;

class Model;

class Firm;

class Region;

class Sector;

class Storage;

class Output {
  private:
    bool write_connection_parameter(const BusinessConnection* b, const settings::hstring& name);
    void write_connection_parameters(const BusinessConnection* b, const settings::SettingsNode& it);
    bool write_consumer_parameter(const Consumer* c, const settings::hstring& name);
    void write_consumer_parameters(const Consumer* c, const settings::SettingsNode& it);
    void write_consumption_connections(const Firm* p, const settings::SettingsNode& it);
    bool write_economic_agent_parameter(const EconomicAgent* p, const settings::hstring& name);
    void write_ingoing_connections(const Storage* s, const settings::SettingsNode& it);
    void write_input_storages(const EconomicAgent* ea, const settings::SettingsNode& it);
    void write_input_storage_parameters(const Storage* s, const settings::SettingsNode& it);
    bool write_input_storage_parameter(const Storage* s, const settings::hstring& name);
    void write_outgoing_connections(const Firm* p, const settings::SettingsNode& it);
    void write_firm_parameters(const Firm* p, const settings::SettingsNode& it);
    bool write_firm_parameter(const Firm* p, const settings::hstring& name);
    void write_region_parameters(const Region* region, const settings::SettingsNode& it);
    bool write_region_parameter(const Region* region, const settings::hstring& name);
    void write_sector_parameters(const Sector* sector, const settings::SettingsNode& parameters);
    bool write_sector_parameter(const Sector* sector, const settings::hstring& name);
    inline void internal_write_value(const hstring& name, const Stock& v);
    inline void internal_write_value(const hstring& name, const Flow& v);
    inline void internal_write_value(const hstring& name, FloatType v);
    template<int precision_digits_p>
    inline void internal_write_value(const hstring& name, const Type<precision_digits_p>& v, const hstring& suffix = hstring::null());

  protected:
    std::string settings_string;
    settings::SettingsNode output_node;
    Model* const model_m;
    std::time_t start_time;
    bool is_first_timestep() const;
    bool is_last_timestep() const;
    inline void parameter_not_found(const std::string& name) const;
    virtual void internal_write_header(tm* timestamp, int max_threads);
    virtual void internal_write_footer(tm* duration);
    virtual void internal_write_settings();
    virtual void internal_start();
    virtual void internal_iterate_begin();
    virtual void internal_iterate_end();
    virtual void internal_end();
    virtual void internal_write_value(const hstring& name, FloatType v, const hstring& suffix);
    virtual void internal_start_target(const hstring& name, Sector* sector, Region* region);
    virtual void internal_start_target(const hstring& name, Sector* sector);
    virtual void internal_start_target(const hstring& name, Region* region);
    virtual void internal_start_target(const hstring& name);
    virtual void internal_end_target();

  public:
    Scenario* const scenario;
    Output(const settings::SettingsNode& settings_p, Model* model_p, Scenario* scenario_p, settings::SettingsNode output_node_p);
    virtual void initialize() = 0;
    virtual void event(EventType type, const Sector* sector_from, const Region* region_from, const Sector* sector_to, const Region* region_to, FloatType value);
    virtual void event(EventType type, const Sector* sector_from, const Region* region_from, const EconomicAgent* economic_agent_to, FloatType value);
    virtual void event(EventType type, const EconomicAgent* economic_agent_from, const EconomicAgent* economic_agent_to, FloatType value);
    virtual void event(EventType type, const EconomicAgent* economic_agent_from, const Sector* sector_to, const Region* region_to, FloatType value);
    void start();
    void iterate();
    void end();

    virtual void flush() {}

    virtual void checkpoint_stop() {}

    virtual void checkpoint_resume() {}

    virtual ~Output() = default;

    inline Model* model() const { return model_m; }

    virtual inline std::string id() const { return "OUTPUT"; }
};
}  // namespace acclimate

#endif
