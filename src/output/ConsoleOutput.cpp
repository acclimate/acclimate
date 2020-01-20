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

#include "output/ConsoleOutput.h"

#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>

#include "model/Model.h"
#include "model/Region.h"
#include "settingsnode.h"
#include "version.h"

namespace acclimate {

ConsoleOutput::ConsoleOutput(const settings::SettingsNode& settings_p, Model* model_p, settings::SettingsNode output_node_p)
    : Output(settings_p, model_p, std::move(output_node_p)) {
    stack = 0;
    out = nullptr;
}

void ConsoleOutput::initialize() {
    if (!output_node.has("file")) {
        out = &std::cout;
    } else {
        const auto filename = output_node["file"].template as<std::string>();
        outfile = std::make_unique<std::ofstream>();
        outfile->open(filename.c_str(), std::ofstream::out);
        out = outfile.get();
    }
}

void ConsoleOutput::internal_write_header(tm* timestamp, unsigned int max_threads) {
    *out << "Start time " << std::asctime(timestamp)
         << "\n"
            "Version "
         << version
         << "\n"
            "Max "
         << max_threads << " threads" << std::endl;
}

void ConsoleOutput::internal_write_footer(tm* duration) { *out << "\n\nDuration " << std::mktime(duration) << "s"; }

void ConsoleOutput::internal_write_settings() { *out << '\n' << settings_string << '\n'; }

void ConsoleOutput::internal_start() {
    *out << "Starting"
            "\n\n"
            "Iteration time "
         << model()->time();
    out->flush();
}

void ConsoleOutput::internal_iterate_begin() {
    *out << "\n\n"
         << "Iteration time " << (model()->time() + Time(1));
    out->flush();
}

void ConsoleOutput::internal_end() {
    *out << "\n\nEnded\n";
    out->flush();
}

void ConsoleOutput::internal_write_value(const hstring& name, FloatType v, const hstring& suffix) {
    *out << std::setprecision(15) << "\t" << name << suffix << "=" << v;
}

void ConsoleOutput::internal_start_target(const hstring& name, Sector* sector, Region* region) {
    ++stack;
    *out << '\n' << std::string(2 * stack, ' ') << name << " " << sector->id() << "," << region->id() << ":";
}

void ConsoleOutput::internal_start_target(const hstring& name, Sector* sector) {
    ++stack;
    *out << '\n' << std::string(2 * stack, ' ') << name << " " << sector->id() << ":";
}

void ConsoleOutput::internal_start_target(const hstring& name, Region* region) {
    ++stack;
    *out << '\n' << std::string(2 * stack, ' ') << name << " " << region->id() << ":";
}

void ConsoleOutput::internal_start_target(const hstring& name) {
    ++stack;
    *out << '\n' << std::string(2 * stack, ' ') << name << ":";
}

void ConsoleOutput::internal_end_target() { --stack; }

}  // namespace acclimate
