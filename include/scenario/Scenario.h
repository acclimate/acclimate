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

#ifndef ACCLIMATE_SCENARIO_H
#define ACCLIMATE_SCENARIO_H

#include <string>

#include "settingsnode.h"
#include "types.h"

namespace acclimate {

class Consumer;
class Firm;
class GeoLocation;
class Model;

class Scenario {
  protected:
    settings::SettingsNode scenario_node;
    const settings::SettingsNode& settings;
    Model* const model_m;

  protected:
    void set_firm_property(Firm* firm, const settings::SettingsNode& node, bool reset);
    void set_consumer_property(Consumer* consumer, const settings::SettingsNode& node, bool reset);
    void set_location_property(GeoLocation* location, const settings::SettingsNode& node, bool reset);
    void apply_target(const settings::SettingsNode& node, bool reset);

  public:
    Scenario(const settings::SettingsNode& settings_p, settings::SettingsNode scenario_node_p, Model* model_p);
    virtual ~Scenario() = default;
    virtual Time start() { return Time(0.0); }  // TODO eliminate return type
    virtual void end() {}
    virtual bool is_first_timestep() const;
    virtual bool iterate();
    virtual std::string calendar_str() const { return "standard"; }
    virtual std::string time_units_str() const;
    Model* model() const { return model_m; }
    virtual std::string id() const { return "SCENARIO"; }
};
}  // namespace acclimate

#endif
