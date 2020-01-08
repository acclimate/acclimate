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

#include "acclimate.h"

#include <cfenv>
#include <csignal>
#include <ostream>
#include <string>
#include <utility>

#include "run.h"
#include "types.h"

namespace acclimate {

#ifdef FLOATING_POINT_EXCEPTIONS
void handle_fpe_error(int /* signal */) {
    int exceptions = fetestexcept(FE_ALL_EXCEPT);
    feclearexcept(FE_ALL_EXCEPT);
    if (exceptions == 0) {
        return;
    }
    if (exceptions & FE_OVERFLOW) {
        warning_("FPE_OVERFLOW");
    }
    if (exceptions & FE_INVALID) {
        warning_("FPE_INVALID");
    }
    if (exceptions & FE_DIVBYZERO) {
        warning_("FPE_DIVBYZERO");
    }
    if (exceptions & FE_INEXACT) {
        warning_("FE_INEXACT");
    }
    if (exceptions & FE_UNDERFLOW) {
        warning_("FE_UNDERFLOW");
    }
#ifdef FATAL_FLOATING_POINT_EXCEPTIONS
    error_("Floating point exception");
#endif
}
#endif

Acclimate::Acclimate(const settings::SettingsNode& settings_p) {
#ifdef BANKERS_ROUNDING
    fesetround(FE_TONEAREST);
#endif
#ifdef FLOATING_POINT_EXCEPTIONS
    signal(SIGFPE, handle_fpe_error);
    feenableexcept(FE_OVERFLOW | FE_INVALID | FE_DIVBYZERO);
#endif
    run_m = std::make_unique<Run>(settings_p);
}

int Acclimate::run() { return (run_m.get())->run(); }

}  // namespace acclimate
