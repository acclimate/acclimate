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

#ifndef ACCLIMATE_CONSOLEOUTPUT_H
#define ACCLIMATE_CONSOLEOUTPUT_H

#include <fstream>
#include <memory>

#include "output/Output.h"

namespace acclimate {

class Model;
class Region;
class Sector;

class ConsoleOutput : public Output {
  private:
    using Output::output_node;
    using Output::settings_string;
    unsigned char stack;
    std::ostream* out;
    std::unique_ptr<std::ofstream> outfile;

  private:
    void internal_write_header(tm* timestamp, unsigned int max_threads) override;
    void internal_write_footer(tm* duration) override;
    void internal_write_settings() override;
    void internal_start() override;
    void internal_iterate_begin() override;
    void internal_end() override;
    void internal_write_value(const hstring& name, FloatType v, const hstring& suffix) override;
    void internal_start_target(const hstring& name, Sector* sector, Region* region) override;
    void internal_start_target(const hstring& name, Sector* sector) override;
    void internal_start_target(const hstring& name, Region* region) override;
    void internal_start_target(const hstring& name) override;
    void internal_end_target() override;

  public:
    ConsoleOutput(const settings::SettingsNode& settings_p, Model* model_p, settings::SettingsNode output_node_p);
    void initialize() override;
    using Output::model;
};
}  // namespace acclimate

#endif
