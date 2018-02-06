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

#ifndef ACCLIMATE_GNUPLOTOUTPUT_H
#define ACCLIMATE_GNUPLOTOUTPUT_H

#include <fstream>
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
class GnuplotOutput : public Output<ModelVariant> {
  public:
    using Output<ModelVariant>::id;
    using Output<ModelVariant>::model;
    using Output<ModelVariant>::output_node;
    using Output<ModelVariant>::settings;

  private:
    std::ofstream file;
    std::unordered_map<Region<ModelVariant>*, int> region_index;
    std::unordered_map<Sector<ModelVariant>*, int> sector_index;
    struct Target {
        int region = 0;
        int sector = 0;
    };
    std::vector<Target> stack;

  protected:
    void internal_write_header(tm* timestamp, int max_threads) override;
    void internal_write_footer(tm* duration) override;
    void internal_write_settings() override;
    void internal_start() override;
    void internal_end() override;
    void internal_write_value(const hstring& name, const FloatType& v, const hstring& suffix) override;
    void internal_start_target(const hstring& name, Sector<ModelVariant>* sector, Region<ModelVariant>* region) override;
    void internal_start_target(const hstring& name, Sector<ModelVariant>* sector) override;
    void internal_start_target(const hstring& name, Region<ModelVariant>* region) override;
    void internal_start_target(const hstring& name) override;
    void internal_end_target() override;

  public:
    GnuplotOutput(const settings::SettingsNode& settings_p,
                  Model<ModelVariant>* model_p,
                  Scenario<ModelVariant>* scenario_p,
                  settings::SettingsNode output_node_p);
    void initialize() override;
};
}  // namespace acclimate

#endif
