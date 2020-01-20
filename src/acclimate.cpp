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

static void handle_fpe_error(int /* signal */) {
    if constexpr (options::FLOATING_POINT_EXCEPTIONS) {
        unsigned int exceptions = fetestexcept(FE_ALL_EXCEPT);  // NOLINT(hicpp-signed-bitwise)
        feclearexcept(FE_ALL_EXCEPT);                           // NOLINT(hicpp-signed-bitwise)
        if (exceptions == 0) {
            return;
        }
        if ((exceptions & FE_OVERFLOW) != 0) {  // NOLINT(hicpp-signed-bitwise)
            warning_("FPE_OVERFLOW");
        }
        if ((exceptions & FE_INVALID) != 0) {  // NOLINT(hicpp-signed-bitwise)
            warning_("FPE_INVALID");
        }
        if ((exceptions & FE_DIVBYZERO) != 0) {  // NOLINT(hicpp-signed-bitwise)
            warning_("FPE_DIVBYZERO");
        }
        if ((exceptions & FE_INEXACT) != 0) {  // NOLINT(hicpp-signed-bitwise)
            warning_("FE_INEXACT");
        }
        if ((exceptions & FE_UNDERFLOW) != 0) {  // NOLINT(hicpp-signed-bitwise)
            warning_("FE_UNDERFLOW");
        }
        if constexpr (options::FATAL_FLOATING_POINT_EXCEPTIONS) {
            error_("Floating point exception");
        }
    }
}

Acclimate::Acclimate(const settings::SettingsNode& settings_p) {
    if constexpr (options::BANKERS_ROUNDING) {
        fesetround(FE_TONEAREST);
    }
    if constexpr (options::FLOATING_POINT_EXCEPTIONS) {
        signal(SIGFPE, handle_fpe_error);
        feenableexcept(FE_OVERFLOW | FE_INVALID | FE_DIVBYZERO);  // NOLINT(hicpp-signed-bitwise)
    }
    run_m = std::make_unique<Run>(settings_p);
}

int Acclimate::run() { return (run_m.get())->run(); }

}  // namespace acclimate
