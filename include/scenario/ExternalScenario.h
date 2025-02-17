// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_EXTERNALSCENARIO_H
#define ACCLIMATE_EXTERNALSCENARIO_H

#include <memory>
#include <string>

#include "acclimate.h"
#include "scenario/Scenario.h"

namespace settings {
class SettingsNode;
}  // namespace settings

namespace acclimate {

class ExternalForcing;  // IWYU pragma: keep
class Model;

class ExternalScenario : public Scenario {
  protected:
    std::string forcing_file_;
    std::string expression_;
    std::string variable_name_;
    bool remove_afterwards_ = false;
    bool done_ = false;
    unsigned int file_index_from_ = 0;
    unsigned int file_index_to_ = 0;
    unsigned int file_index_ = 0;
    std::string calendar_str_;
    std::string time_units_str_;
    Time next_time_ = Time(0.0);
    Time time_offset_ = Time(0.0);
    int time_step_width_ = 1;
    std::unique_ptr<ExternalForcing> forcing_;

  protected:
    ExternalScenario(const settings::SettingsNode& settings, settings::SettingsNode scenario_node, Model* model);
    bool next_forcing_file();
    std::string fill_template(const std::string& in) const;
    unsigned int get_ref_year(const std::string& filename, const std::string& time_str);
    virtual void internal_start() {}
    virtual void internal_iterate_start() {}
    virtual void internal_iterate_end() {}
    virtual void iterate_first_timestep() {}
    virtual ExternalForcing* read_forcing_file(const std::string& filename, const std::string& variable_name) = 0;
    virtual void read_forcings() = 0;

  public:
    virtual ~ExternalScenario() override = default;
    void iterate() override;
    void start() override;
    void end() override;
    std::string calendar_str() const override { return calendar_str_; }
    std::string time_units_str() const override { return time_units_str_; }
};
}  // namespace acclimate

#endif
