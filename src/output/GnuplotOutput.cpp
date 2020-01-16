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
#include "model/Region.h"
#include "run.h"
#include "settingsnode.h"
#include "version.h"

namespace acclimate {

GnuplotOutput::GnuplotOutput(const settings::SettingsNode& settings_p, Model* model_p, Scenario* scenario_p, settings::SettingsNode output_node_p)
    : Output(settings_p, model_p, scenario_p, std::move(output_node_p)) {}

void GnuplotOutput::initialize() {
    if (!output_node.has("file")) {
        error("Output file name not given");
    }
    const auto filename = output_node["file"].template as<std::string>();
    file.open(filename.c_str(), std::ofstream::out);
}

void GnuplotOutput::internal_write_header(tm* timestamp, unsigned int max_threads) {
    file << "# Start time: " << std::asctime(timestamp) << "# Version: " << version << "\n"
         << "# Max number of threads: " << max_threads << "\n";
}

void GnuplotOutput::internal_write_footer(tm* duration) { file << "# Duration: " << std::mktime(duration) << "s\n"; }

void GnuplotOutput::internal_write_settings() {
    std::stringstream ss;
    ss << settings_string;
    ss.flush();
    ss.seekg(0);
    std::string line;
    file << "# Settings:\n";
    while (getline(ss, line)) {
        file << "# " << line << "\n";
    }
    file << "#\n";
}

void GnuplotOutput::internal_end() { file.close(); }

void GnuplotOutput::internal_start() {
    file << "# Sectors:\n# set ytics (";
    for (std::size_t i = 0; i < model()->sectors.size(); ++i) {
        file << "\"" << model()->sectors[i]->id() << "\" " << i;
        if (i < model()->sectors.size() - 1) {
            file << ", ";
        }
        sector_index.emplace(model()->sectors[i].get(), i);
    }
    file << ")\n# Regions:\n# set ytics (";
    for (std::size_t i = 0; i < model()->regions.size(); ++i) {
        file << "\"" << model()->regions[i]->id() << "\" " << i;
        if (i < model()->regions.size() - 1) {
            file << ", ";
        }
        region_index.emplace(model()->regions[i].get(), i);
    }
    file << ")\n";
}

void GnuplotOutput::internal_write_value(const hstring& name, FloatType v, const hstring& suffix) {
    UNUSED(name);
    UNUSED(suffix);
    file << model()->time() << " ";
    for (const auto& it : stack) {
        if (it.region >= 0) {
            if (it.sector >= 0) {
                file << it.sector << " " << it.region << " ";
            } else {
                file << it.region << " ";
            }
        } else {
            if (it.sector >= 0) {
                file << it.sector << " ";
            }
        }
    }
    file << v << "\n";
}

void GnuplotOutput::internal_start_target(const hstring& name, Sector* sector, Region* region) {
    UNUSED(name);
    Target t;
    t.sector = sector_index[sector];
    t.region = region_index[region];
    stack.push_back(t);
}

void GnuplotOutput::internal_start_target(const hstring& name, Sector* sector) {
    UNUSED(name);
    Target t;
    t.sector = sector_index[sector];
    t.region = -1;
    stack.push_back(t);
}

void GnuplotOutput::internal_start_target(const hstring& name, Region* region) {
    UNUSED(name);
    Target t;
    t.sector = -1;
    t.region = region_index[region];
    stack.push_back(t);
}

void GnuplotOutput::internal_start_target(const hstring& name) {
    UNUSED(name);
    Target t;
    t.sector = -1;
    t.region = -1;
    stack.push_back(t);
}

void GnuplotOutput::internal_end_target() { stack.pop_back(); }

}  // namespace acclimate
