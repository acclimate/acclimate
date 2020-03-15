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

#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "ModelRun.h"
#include "acclimate.h"
#include "settingsnode.h"
#include "settingsnode/inner.h"
#include "settingsnode/yaml.h"
#include "version.h"

namespace acclimate {
extern const char* info;
}  // namespace acclimate

static void print_usage(const char* program_name) {
    std::cerr << "Acclimate model\n"
                 "Version: "
              << acclimate::version
              << "\n\n"
                 "Authors: Sven Willner <sven.willner@pik-potsdam.de>\n"
                 "         Christian Otto <christian.otto@pik-potsdam.de>\n"
                 "\n"
                 "Usage:   "
              << program_name
              << " (<option> | <settingsfile>)\n"
                 "Options:\n"
              << (acclimate::has_diff ? "  -d, --diff     Print git diff output from compilation\n" : "")
              << "  -h, --help     Print this help text\n"
                 "  -i, --info     Print further information\n"
                 "  -v, --version  Print version"
              << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    const std::string arg = argv[1];
    if (arg.length() > 1 && arg[0] == '-') {
        if (arg == "--version" || arg == "-v") {
            std::cout << acclimate::version << std::endl;
        } else if (arg == "--info" || arg == "-i") {
            std::cout << "Version:                " << acclimate::version << "\n\n"
                      << acclimate::info
                      << "\n"
                         "Precision Time:         "
                      << acclimate::Time::precision_digits
                      << "\n"
                         "Precision Quantity:     "
                      << acclimate::Quantity::precision_digits
                      << "\n"
                         "Precision FlowQuantity: "
                      << acclimate::FlowQuantity::precision_digits
                      << "\n"
                         "Precision Price:        "
                      << acclimate::Price::precision_digits
                      << "\n"
                         "Options:                ";
            bool first = true;
            for (const auto& option : acclimate::options::options) {
                if (first) {
                    first = false;
                } else {
                    std::cout << "                        ";
                }
                std::cout << option.name << " = " << (option.value ? "true" : "false") << "\n";
            }
            std::cout << std::flush;
        } else if (acclimate::has_diff && (arg == "--diff" || arg == "-d")) {
            std::cout << acclimate::git_diff << std::flush;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
        } else {
            print_usage(argv[0]);
            return 1;
        }
    } else {
        try {
            if (arg == "-") {
                std::cin >> std::noskipws;
                acclimate::ModelRun acclimate(settings::SettingsNode(std::make_unique<settings::YAML>(std::cin)));
                acclimate.run();
            } else {
                std::ifstream settings_file(arg);
                if (!settings_file) {
                    throw std::runtime_error("Cannot open " + arg);
                }
                acclimate::ModelRun acclimate(settings::SettingsNode(std::make_unique<settings::YAML>(settings_file)));
                acclimate.run();
            }
        } catch (const acclimate::return_after_checkpoint&) {
            return 7;
        } catch (const std::exception& ex) {
            std::cerr << ex.what() << std::endl;
            return 255;
        }
    }
    return 0;
}
