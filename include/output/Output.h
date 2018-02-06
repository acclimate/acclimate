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
#include "acclimate.h"
#include "settingsnode.h"

namespace acclimate {

using hstring = settings::hstring;

template<class ModelVariant>
class Scenario;
template<class ModelVariant>
class BusinessConnection;
template<class ModelVariant>
class Consumer;
template<class ModelVariant>
class EconomicAgent;
template<class ModelVariant>
class Model;
template<class ModelVariant>
class Firm;
template<class ModelVariant>
class Region;
template<class ModelVariant>
class Sector;
template<class ModelVariant>
class Storage;

template<class ModelVariant>
class Output {
  private:
    bool write_connection_parameter_variant(const BusinessConnection<ModelVariant>* b, const settings::hstring& name);
    void write_connection_parameters(const BusinessConnection<ModelVariant>* b, const settings::SettingsNode& it);
    bool write_consumer_parameter_variant(const Consumer<ModelVariant>* c, const settings::hstring& name);
    void write_consumer_parameters(const Consumer<ModelVariant>* c, const settings::SettingsNode& it);
    void write_consumption_connections(const Firm<ModelVariant>* p, const settings::SettingsNode& it);
    bool write_economic_agent_parameter(const EconomicAgent<ModelVariant>* p, const settings::hstring& name);
    void write_ingoing_connections(const Storage<ModelVariant>* s, const settings::SettingsNode& it);
    void write_input_storages(const EconomicAgent<ModelVariant>* ea, const settings::SettingsNode& it);
    void write_input_storage_parameters(const Storage<ModelVariant>* s, const settings::SettingsNode& it);
    bool write_input_storage_parameter_variant(const Storage<ModelVariant>* s, const settings::hstring& name);
    void write_outgoing_connections(const Firm<ModelVariant>* p, const settings::SettingsNode& it);
    void write_firm_parameters(const Firm<ModelVariant>* p, const settings::SettingsNode& it);
    bool write_firm_parameter_variant(const Firm<ModelVariant>* p, const settings::hstring& name);
    void write_region_parameters(const Region<ModelVariant>* region, const settings::SettingsNode& it);
    bool write_region_parameter_variant(const Region<ModelVariant>* region, const settings::hstring& name);
    void write_sector_parameters(const Sector<ModelVariant>* sector, const settings::SettingsNode& parameters);
    bool write_sector_parameter_variant(const Sector<ModelVariant>* sector, const settings::hstring& name);
    inline void internal_write_value(const hstring& name, const Stock& v);
    inline void internal_write_value(const hstring& name, const Flow& v);
    inline void internal_write_value(const hstring& name, const FloatType& v);
    template<int precision_digits_p>
    inline void internal_write_value(const hstring& name, const Type<precision_digits_p>& v, const hstring& suffix = hstring::null());

  protected:
    const settings::SettingsNode& settings;
    const settings::SettingsNode output_node;
    time_t start_time;
    inline bool is_first_timestep() const { return scenario->is_first_timestep(); };
    inline bool is_last_timestep() const { return scenario->is_last_timestep(); };
    inline void parameter_not_found(const std::string& name) const;
    virtual void internal_write_header(tm* timestamp, int max_threads);
    virtual void internal_write_footer(tm* duration);
    virtual void internal_write_settings();
    virtual void internal_start();
    virtual void internal_iterate_begin();
    virtual void internal_iterate_end();
    virtual void internal_end();
    virtual void internal_write_value(const hstring& name, const FloatType& v, const hstring& suffix);
    virtual void internal_start_target(const hstring& name, Sector<ModelVariant>* sector, Region<ModelVariant>* region);
    virtual void internal_start_target(const hstring& name, Sector<ModelVariant>* sector);
    virtual void internal_start_target(const hstring& name, Region<ModelVariant>* region);
    virtual void internal_start_target(const hstring& name);
    virtual void internal_end_target();

  public:
    Model<ModelVariant>* const model;
    Scenario<ModelVariant>* const scenario;
    Output(const settings::SettingsNode& settings_p,
           Model<ModelVariant>* model_p,
           Scenario<ModelVariant>* scenario_p,
           settings::SettingsNode output_node_p);
    virtual void initialize() = 0;
    virtual void event(const EventType& type,
                       const Sector<ModelVariant>* sector_from,
                       const Region<ModelVariant>* region_from,
                       const Sector<ModelVariant>* sector_to,
                       const Region<ModelVariant>* region_to,
                       const FloatType& value);
    virtual void event(const EventType& type,
                       const Sector<ModelVariant>* sector_from,
                       const Region<ModelVariant>* region_from,
                       const EconomicAgent<ModelVariant>* economic_agent_to,
                       const FloatType& value);
    virtual void event(const EventType& type,
                       const EconomicAgent<ModelVariant>* economic_agent_from,
                       const EconomicAgent<ModelVariant>* economic_agent_to,
                       const FloatType& value);
    virtual void event(const EventType& type,
                       const EconomicAgent<ModelVariant>* economic_agent_from,
                       const Sector<ModelVariant>* sector_to,
                       const Region<ModelVariant>* region_to,
                       const FloatType& value);
    void start();
    void iterate();
    void end();
    virtual void flush() {};
    virtual void checkpoint_stop() {};
    virtual void checkpoint_resume() {};
    virtual ~Output() = default;
    virtual inline std::string id() const { return "OUTPUT"; }
};
}  // namespace acclimate

#endif
