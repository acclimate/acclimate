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

#include "output/GnuplotOutput.h"
#include <cstddef>
#include <ctime>
#include <string>
#include <utility>
#include "model/Model.h"
#include "run.h"
#include "settingsnode.h"
#include "variants/ModelVariants.h"
#include "version.h"

namespace acclimate {

template<class ModelVariant>
GnuplotOutput<ModelVariant>::GnuplotOutput(const settings::SettingsNode& settings_p,
                                           Model<ModelVariant>* model_p,
                                           Scenario<ModelVariant>* scenario_p,
                                           settings::SettingsNode output_node_p)
    : Output<ModelVariant>(settings_p, model_p, scenario_p, std::move(output_node_p)) {}

template<class ModelVariant>
void GnuplotOutput<ModelVariant>::initialize() {
    if (!output_node.has("file")) {
        error("Output file name not given");
    }
    std::string filename = output_node["file"].template as<std::string>();
    file.open(filename.c_str(), std::ofstream::out);
}

template<class ModelVariant>
void GnuplotOutput<ModelVariant>::internal_write_header(tm* timestamp, int max_threads) {
    file << "# Start time: " << std::asctime(timestamp) << "# Version: " << ACCLIMATE_VERSION << "\n"
         << "# Max number of threads: " << max_threads << "\n";
}

template<class ModelVariant>
void GnuplotOutput<ModelVariant>::internal_write_footer(tm* duration) {
    file << "# Duration: " << std::mktime(duration) << "s\n";
}

template<class ModelVariant>
void GnuplotOutput<ModelVariant>::internal_write_settings() {
    std::stringstream ss;
    ss << settings;
    ss.seekg(0);
    std::string line;
    file << "# Settings:\n";
    while (getline(ss, line)) {
        file << "# " << line << "\n";
    }
    file << "#\n";
}

template<class ModelVariant>
void GnuplotOutput<ModelVariant>::internal_end() {
    file.close();
}

template<class ModelVariant>
void GnuplotOutput<ModelVariant>::internal_start() {
    file << "# Sectors:\n# set ytics (";
    for (std::size_t i = 0; i < model()->sectors_C.size(); ++i) {
        file << "\"" << model()->sectors_C[i]->id() << "\" " << i;
        if (i < model()->sectors_C.size() - 1) {
            file << ", ";
        }
        sector_index.emplace(model()->sectors_C[i].get(), i);
    }
    file << ")\n# Regions:\n# set ytics (";
    for (std::size_t i = 0; i < model()->regions_R.size(); ++i) {
        file << "\"" << model()->regions_R[i]->id() << "\" " << i;
        if (i < model()->regions_R.size() - 1) {
            file << ", ";
        }
        region_index.emplace(model()->regions_R[i].get(), i);
    }
    file << ")\n";
}

template<class ModelVariant>
void GnuplotOutput<ModelVariant>::internal_write_value(const hstring& name, FloatType v, const hstring& suffix) {
    UNUSED(name);
    UNUSED(suffix);
    file << model()->time() << " ";
    for (auto it = stack.begin(); it != stack.end(); ++it) {
        if (it->region >= 0) {
            if (it->sector >= 0) {
                file << it->sector << " " << it->region << " ";
            } else {
                file << it->region << " ";
            }
        } else {
            if (it->sector >= 0) {
                file << it->sector << " ";
            }
        }
    }
    file << v << "\n";
}

template<class ModelVariant>
void GnuplotOutput<ModelVariant>::internal_start_target(const hstring& name, Sector<ModelVariant>* sector, Region<ModelVariant>* region) {
    UNUSED(name);
    Target t;
    t.sector = sector_index[sector];
    t.region = region_index[region];
    stack.push_back(t);
}

template<class ModelVariant>
void GnuplotOutput<ModelVariant>::internal_start_target(const hstring& name, Sector<ModelVariant>* sector) {
    UNUSED(name);
    Target t;
    t.sector = sector_index[sector];
    t.region = -1;
    stack.push_back(t);
}

template<class ModelVariant>
void GnuplotOutput<ModelVariant>::internal_start_target(const hstring& name, Region<ModelVariant>* region) {
    UNUSED(name);
    Target t;
    t.sector = -1;
    t.region = region_index[region];
    stack.push_back(t);
}

template<class ModelVariant>
void GnuplotOutput<ModelVariant>::internal_start_target(const hstring& name) {
    UNUSED(name);
    Target t;
    t.sector = -1;
    t.region = -1;
    stack.push_back(t);
}

template<class ModelVariant>
void GnuplotOutput<ModelVariant>::internal_end_target() {
    stack.pop_back();
}

INSTANTIATE_BASIC(GnuplotOutput);
}  // namespace acclimate
