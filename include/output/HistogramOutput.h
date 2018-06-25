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

#ifndef ACCLIMATE_HISTOGRAMOUTPUT_H
#define ACCLIMATE_HISTOGRAMOUTPUT_H

#include <fstream>
#include <vector>
#include "output/Output.h"
#include "types.h"

namespace acclimate {

template<class ModelVariant>
class Model;
template<class ModelVariant>
class Scenario;

template<class ModelVariant>
class HistogramOutput : public Output<ModelVariant> {
  public:
    using Output<ModelVariant>::output_node;
    using Output<ModelVariant>::model;
    using Output<ModelVariant>::settings;

  private:
    std::ofstream file;
    bool exclude_max;
    unsigned int windows;
    double min, max;
    std::vector<unsigned int> count;

  protected:
    void internal_write_header(tm* timestamp, int max_threads) override;
    void internal_write_footer(tm* duration) override;
    void internal_write_settings() override;
    void internal_iterate_begin() override;
    void internal_iterate_end() override;
    void internal_end() override;
    void internal_write_value(const hstring& name, FloatType v, const hstring& suffix) override;

  public:
    HistogramOutput(const settings::SettingsNode& settings_p,
                    Model<ModelVariant>* model_p,
                    Scenario<ModelVariant>* scenario_p,
                    settings::SettingsNode output_node_p);
    void initialize() override;
};
}  // namespace acclimate

#endif
