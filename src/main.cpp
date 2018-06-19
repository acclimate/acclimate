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

#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include "acclimate.h"
#include "settingsnode.h"
#include "settingsnode/yaml.h"
#include "version.h"

#ifdef ACCLIMATE_HAS_DIFF
extern const char* acclimate_git_diff;
#endif

static void print_usage(const char* program_name) {
    std::cerr << "Acclimate model\n"
                 "Version: " ACCLIMATE_VERSION
                 "\n\n"
                 "Authors: Sven Willner <sven.willner@pik-potsdam.de>\n"
                 "         Christian Otto <christian.otto@pik-potsdam.de>\n"
                 "         Kilian Kuhla <kilian.kuhla@pik-potsdam.de>\n"
                 "\n"
                 "Usage:   "
              << program_name
              << " (<option> | <settingsfile>)\n"
                 "Options:\n"
#ifdef ACCLIMATE_HAS_DIFF
                 "  -d, --diff     Print git diff output from compilation\n"
#endif
                 "  -h, --help     Print this help text\n"
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
            std::cout << ACCLIMATE_VERSION << std::endl;
        } else if (arg == "--info" || arg == "-i") {
            std::cout << "Version:                " ACCLIMATE_VERSION
                         "\n"
                         "Authors:                Sven Willner <sven.willner@pik-potsdam.de>\n"
                         "                        Christian Otto <christian.otto@pik-potsdam.de>\n"
                         "\n"
                         "Citation:               C. Otto, S.N. Willner, L. Wenz, F. Frieler, A. Levermann (2017).\n"
                         "                        Modeling loss-propagation in the global supply network: The dynamic\n"
                         "                        agent-based model acclimate. J. Econ. Dyn. Control. 83, 232-269.\n"
                         "\n"
                         "Source:                 https://github.com/acclimate/acclimate\n"
                         "License:                AGPL, (c) 2014-2017 S. Willner, C. Otto (see LICENSE file)\n"
                         "\n"
                         "Build time:             " __DATE__ " " __TIME__
                         "\n"
                         "Debug:                  "
#ifdef DEBUG
                         "yes"
#else
                         "no"
#endif
                         "\n"
                         "Parallelized:           "
#ifdef _OPENMP
                         "yes"
#else
                         "no"
#endif
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
                         "Enabled variants:       " ACCLIMATE_VARIANTS
                         "\n"
                         "Enabled options:        " ACCLIMATE_OPTIONS
                      << std::endl;
#ifdef ACCLIMATE_HAS_DIFF
        } else if (arg == "--diff" || arg == "-d") {
            std::cout << acclimate_git_diff << std::flush;
#endif
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
        } else {
            print_usage(argv[0]);
            return 1;
        }
    } else {
#ifndef DEBUG
        try {
#endif
            if (arg == "-") {
                std::cin >> std::noskipws;
                acclimate::Acclimate::initialize(settings::SettingsNode(std::unique_ptr<settings::YAML>(new settings::YAML(std::cin))));
            } else {
                std::ifstream settings_file(arg);
                acclimate::Acclimate::initialize(settings::SettingsNode(std::unique_ptr<settings::YAML>(new settings::YAML(settings_file))));
            }
            const int res = acclimate::Acclimate::instance()->run();
            if (res == 0) {
                acclimate::Acclimate::instance()->cleanup();
            } else {
                acclimate::Acclimate::instance()->memory_cleanup();
            }
            return res;
#ifndef DEBUG
        } catch (std::runtime_error& ex) {
            std::cerr << ex.what() << std::endl;
            acclimate::Acclimate::instance()->memory_cleanup();
            return 255;
        }
#endif
    }
    return 0;
}
