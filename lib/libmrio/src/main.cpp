/*
  Copyright (C) 2014-2017 Sven Willner <sven.willner@pik-potsdam.de>

  This file is part of libmrio.

  libmrio is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  libmrio is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with libmrio.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <exception>
#include <fstream>  // IWYU pragma: keep
#include <iostream>
#include <stdexcept>
#include <string>
#include "Disaggregation.h"
#include "MRIOTable.h"
#include "settingsnode.h"
#include "settingsnode/yaml.h"
#include "version.h"

// Define types used in templates
using I = std::size_t;  // Index type
using T = double;       // Data type

static void print_usage(const char* program_name) {
    std::cerr << "Regional and sectoral disaggregation of multi-regional input-output tables\n"
                 "Version:  " MRIO_DISAGGREGATE_VERSION
                 "\n"
                 "Author:   Sven Willner <sven.willner@pik-potsdam.de>\n\n"
                 "Algorithm described in:\n"
                 "   L. Wenz, S.N. Willner, A. Radebach, R. Bierkandt, J.C. Steckel, A. Levermann.\n"
                 "   Regional and sectoral disaggregation of multi-regional input-output tables:\n"
                 "   a flexible algorithm. Economic Systems Research 27 (2015).\n"
                 "   DOI: 10.1080/09535314.2014.987731\n\n"
                 "Source:   https://github.com/swillner/libmrio\n"
                 "License:  AGPL, (c) 2014-2017 Sven Willner (see LICENSE file)\n\n"
                 "Usage:    "
              << program_name
              << " (<option> | <settingsfile>)\n"
                 "Options:\n"
                 "   -h, --help     Print this help text\n"
                 "   -v, --version  Print version"
              << std::endl;
}

int main(int argc, char* argv[]) {
#ifndef DEBUG
    try {
#endif
        if (argc != 2) {
            print_usage(argv[0]);
            return 1;
        }
        const std::string arg = argv[1];
        if (arg.length() > 1 && arg[0] == '-') {
            if (arg == "--version" || arg == "-v") {
                std::cout << MRIO_DISAGGREGATE_VERSION << std::endl;
            } else if (arg == "--help" || arg == "-h") {
                print_usage(argv[0]);
            } else {
                print_usage(argv[0]);
                return 1;
            }
        } else {
            settings::SettingsNode settings;
            if (arg == "-") {
                std::cin >> std::noskipws;
                settings = settings::SettingsNode(std::unique_ptr<settings::YAML>(new settings::YAML(std::cin)));
            } else {
                std::ifstream settings_file(arg);
                settings = settings::SettingsNode(std::unique_ptr<settings::YAML>(new settings::YAML(settings_file)));
            }

            std::cout << "Loading basetable... " << std::flush;
            mrio::Table<T, I> basetable;
            {
                const std::string& type = settings["basetable"]["type"].as<std::string>();
                const std::string& filename = settings["basetable"]["file"].as<std::string>();
                const auto threshold = settings["basetable"]["threshold"].as<T>();
                if (type == "csv") {
                    std::ifstream indices(settings["basetable"]["index"].as<std::string>());
                    if (!indices) {
                        throw std::runtime_error("Could not open indices file");
                    }
                    std::ifstream data(filename);
                    if (!data) {
                        throw std::runtime_error("Could not open data file");
                    }
                    basetable.read_from_csv(indices, data, threshold);
                } else if (type == "mrio") {
                    std::ifstream data(filename);
                    if (!data) {
                        throw std::runtime_error("Could not open data file");
                    }
                    basetable.read_from_mrio(data, threshold);
#ifdef LIBMRIO_WITH_NETCDF
                } else if (type == "netcdf") {
                    basetable.read_from_netcdf(filename, threshold);
#endif
                } else {
                    throw std::runtime_error("Unknown type '" + type + "'");
                }
            }
            std::cout << "done" << std::endl;

            mrio::Disaggregation<T, I> disaggregation(&basetable);

            std::cout << "Loading proxies... " << std::flush;
            disaggregation.initialize(settings["disaggregation"]);
            std::cout << "done" << std::endl;

            std::cout << "Applying disaggregation algorithm... " << std::flush;
            disaggregation.refine();
            std::cout << "done" << std::endl;

            std::cout << "Writing disaggregated table... " << std::flush;
            {
                const std::string& type = settings["output"]["type"].as<std::string>();
                const std::string& filename = settings["output"]["file"].as<std::string>();
                if (type == "csv") {
                    std::ofstream outfile(filename);
                    if (!outfile) {
                        throw std::runtime_error("Could not create output file");
                    }
                    disaggregation.refined_table().write_to_csv(outfile);
                } else if (type == "mrio") {
                    std::ofstream outfile(filename);
                    if (!outfile) {
                        throw std::runtime_error("Could not create output file");
                    }
                    disaggregation.refined_table().write_to_mrio(outfile);
#ifdef LIBMRIO_WITH_NETCDF
                } else if (type == "netcdf") {
                    disaggregation.refined_table().write_to_netcdf(filename);
#endif
                } else {
                    throw std::runtime_error("Unknown type '" + type + "'");
                }
            }
            std::cout << "done" << std::endl;
        }
#ifndef DEBUG
    } catch (std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return 255;
    }
#endif
    return 0;
}
