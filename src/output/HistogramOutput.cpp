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

#include "output/HistogramOutput.h"
#include <sstream>
#include "model/Model.h"
#include "scenario/Scenario.h"
#include "variants/ModelVariants.h"
#include "version.h"

namespace acclimate {

template<class ModelVariant>
HistogramOutput<ModelVariant>::HistogramOutput(const settings::SettingsNode& settings_p,
                                               Model<ModelVariant>* model_p,
                                               Scenario<ModelVariant>* scenario_p,
                                               settings::SettingsNode output_node_p)
    : Output<ModelVariant>(settings_p, model_p, scenario_p, std::move(output_node_p)) {
    windows = 0;
    min = 0;
    max = 1;
    exclude_max = false;
}

template<class ModelVariant>
void HistogramOutput<ModelVariant>::initialize() {
    min = output_node["windows"]["min"].template as<double>();
    max = output_node["windows"]["max"].template as<double>();
    exclude_max = output_node["windows"]["exclude_max"].template as<bool>();
    windows = output_node["windows"]["count"].template as<int>();
    const std::string filename = output_node["file"].template as<std::string>();
    file.open(filename.c_str(), std::ofstream::out);
    count.resize(windows);
}

template<class ModelVariant>
void HistogramOutput<ModelVariant>::internal_write_header(tm* timestamp, int max_threads) {
    file << "# Start time: " << asctime(timestamp) << "# Version: " << ACCLIMATE_VERSION << "\n"
         << "# Max number of threads: " << max_threads << "\n";
}

template<class ModelVariant>
void HistogramOutput<ModelVariant>::internal_write_footer(tm* duration) {
    file << "# Duration: " << mktime(duration) << "s\n";
}

template<class ModelVariant>
void HistogramOutput<ModelVariant>::internal_write_settings() {
    std::stringstream ss;
    ss << settings;
    ss.flush();
    ss.seekg(0);
    std::string line;
    file << "# Settings:\n";
    while (getline(ss, line)) {
        file << "# " << line << "\n";
    }
    file << "#\n";
}

template<class ModelVariant>
void HistogramOutput<ModelVariant>::internal_iterate_begin() {
    std::fill(std::begin(count), std::end(count), 0);
}

template<class ModelVariant>
void HistogramOutput<ModelVariant>::internal_iterate_end() {
    for (std::size_t i = 0; i < windows; ++i) {
        file << model->time() << " " << (min + i * (max - min) / (windows - 1)) << " " << count[i] << "\n";
    }
    file << "\n";
}

template<class ModelVariant>
void HistogramOutput<ModelVariant>::internal_end() {
    file.close();
}

template<class ModelVariant>
void HistogramOutput<ModelVariant>::internal_write_value(const hstring& name, FloatType v, const hstring& suffix) {
    UNUSED(name);
    UNUSED(suffix);
    if (v >= min && (v < max || !exclude_max)) {
        ++count[iround((v - min) * (windows - 1) / (max - min))];
    }
}

INSTANTIATE_BASIC(HistogramOutput);
}  // namespace acclimate
