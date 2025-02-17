// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_SCENARIO_H
#define ACCLIMATE_SCENARIO_H

#include <string>

#include "acclimate.h"
#include "settingsnode.h"

namespace acclimate {

class Consumer;
class Firm;
class GeoLocation;
class Model;

class Scenario {
  protected:
    settings::SettingsNode scenario_node_;
    const settings::SettingsNode& settings_;
    non_owning_ptr<Model> model_;

  protected:
    static void set_firm_property(Firm* firm, const settings::SettingsNode& node, bool reset);
    static void set_consumer_property(Consumer* consumer, const settings::SettingsNode& node, bool reset);
    static void set_location_property(GeoLocation* location, const settings::SettingsNode& node, bool reset);
    void apply_target(const settings::SettingsNode& node, bool reset);

  public:
    Scenario(const settings::SettingsNode& settings, settings::SettingsNode scenario_node, Model* model);
    virtual ~Scenario() = default;
    virtual void start() {}
    virtual void end() {}
    virtual void iterate();
    virtual std::string calendar_str() const { return "standard"; }
    virtual std::string time_units_str() const;

    Model* model() { return model_; }
    const Model* model() const { return model_; }
    virtual std::string name() const { return "SCENARIO"; }
};
}  // namespace acclimate

#endif
