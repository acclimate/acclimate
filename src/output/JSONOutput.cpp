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

#include "output/JSONOutput.h"

#include <ctime>
#include <iostream>
#include <string>
#include <utility>

#include "model/Model.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "settingsnode.h"
#include "version.h"

namespace acclimate {

JSONOutput::JSONOutput(const settings::SettingsNode& settings_p, Model* model_p, settings::SettingsNode output_node_p)
    : Output(settings_p, model_p, std::move(output_node_p)) {}

void JSONOutput::initialize() {
    if (!output_node.has("file")) {
        out = &std::cout;
    } else {
        const auto filename = output_node["file"].template as<std::string>();
        outfile = std::make_unique<std::ofstream>();
        outfile->open(filename.c_str(), std::ofstream::out);
        out = outfile.get();
    }
}

void JSONOutput::internal_write_header(tm* timestamp, unsigned int max_threads) {
    *out << "{\n"
            "\"info_header\": {\n"
            "    \"start_time\": \""
         << std::asctime(timestamp)
         << "\",\n"
            "    \"version\": \""
         << version
         << "\",\n"
            "    \"max_threads\": "
         << max_threads
         << ",\n"
            "},\n";
}

void JSONOutput::internal_write_footer(tm* duration) {
    *out << "},\n"
            "\"info_footer\": {\n"
            "    \"duration\": \""
         << std::mktime(duration)
         << "s\"\n"
            "}\n";
}

void JSONOutput::internal_write_settings() {
    *out << "    \"settings\": '" << settings_string
         << "'\n},\n"
            "\"data\": {\n";
}

void JSONOutput::internal_iterate_begin() { *out << "    \"" << model()->time() << "\": {\n"; }

void JSONOutput::internal_iterate_end() { *out << "    },\n"; }

void JSONOutput::internal_end() {
    *out << "}\n";
    out->flush();
}

void JSONOutput::internal_write_value(const hstring& name, FloatType v, const hstring& suffix) {
    *out << "            \"" << name << suffix << "\": " << v << ",\n";
}

void JSONOutput::internal_end_target() { *out << "        },\n"; }

void JSONOutput::internal_start_target(const hstring& name, Sector* sector, Region* region) {
    *out << "        \"" << name
         << "\": {\n"
            "            \"sector\": \""
         << sector->id()
         << "\",\n"
            "            \"region\": \""
         << region->id() << "\",\n";
}

void JSONOutput::internal_start_target(const hstring& name, Sector* sector) {
    *out << "        \"" << name
         << "\": {\n"
            "            \"sector\": \""
         << sector->id() << "\",\n";
}

void JSONOutput::internal_start_target(const hstring& name, Region* region) {
    *out << "        \"" << name
         << "\": {\n"
            "            \"region\": \""
         << region->id() << "\",\n";
}

void JSONOutput::internal_start_target(const hstring& name) { *out << "        \"" << name << "\": {\n"; }

}  // namespace acclimate
