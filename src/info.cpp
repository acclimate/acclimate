/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
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

namespace acclimate {

const char* info =
    "Authors:                Sven Willner <sven.willner@pik-potsdam.de>\n"
    "                        Christian Otto <christian.otto@pik-potsdam.de>\n"
    "\n"
    "Contributors:           Kilian Kuhla <kilian.kuhla@pik-potsdam.de>\n"
    "                        Patryk Kubiczek <patryk.kubiczek@pik-potsdam.de>\n"
    "\n"
    "Citation:               C. Otto, S.N. Willner, L. Wenz, F. Frieler, A. Levermann (2017).\n"
    "                        Modeling loss-propagation in the global supply network: The dynamic\n"
    "                        agent-based model acclimate. J. Econ. Dyn. Control. 83, 232-269.\n"
    "\n"
    "Source:                 https://github.com/acclimate/acclimate\n"
    "License:                AGPL, (c) 2014-2020 S. Willner, C. Otto (see LICENSE file)\n"
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
