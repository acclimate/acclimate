// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

namespace acclimate {

struct Info {
    static const char* const text;
};

const char* const Info::text =
    "Authors:                Sven Willner <sven.willner@pik-potsdam.de>\n"
    "                        Christian Otto <christian.otto@pik-potsdam.de>\n"
    "\n"
    "Contributors:           Kilian Kuhla <kilian.kuhla@pik-potsdam.de>\n"
    "                        Patryk Kubiczek <patryk.kubiczek@pik-potsdam.de>\n"
    "                        Lennart Quante <lennart.quante@pik-potsdam.de>\n"
    "                        Robin Middelanis <robin.middelanis@pik-potsdam.de>\n"
    "\n"
    "Citation:               C. Otto, S.N. Willner, L. Wenz, F. Frieler, A. Levermann (2017).\n"
    "                        Modeling loss-propagation in the global supply network: The dynamic\n"
    "                        agent-based model acclimate. J. Econ. Dyn. Control. 83, 232-269.\n"
    "\n"
    "Source:                 https://github.com/acclimate/acclimate\n"
    "License:                AGPL-3.0-or-later (see LICENSE file)\n"
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
    ;

}  // namespace acclimate
