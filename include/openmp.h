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

#ifndef ACCLIMATE_OPENMP_H
#define ACCLIMATE_OPENMP_H

#ifdef _OPENMP
#include <omp.h>
#endif

namespace acclimate {

class OpenMPLock {
  protected:
#ifdef _OPENMP
    omp_lock_t lock = {};
#endif
  public:
    OpenMPLock() {
#ifdef _OPENMP
        omp_init_lock(&lock);
#endif
    }

    ~OpenMPLock() {
#ifdef _OPENMP
        omp_destroy_lock(&lock);
#endif
    }

    template<typename Func>
    inline void call(const Func& f) {
#ifdef _OPENMP
        omp_set_lock(&lock);
#endif
        f();
#ifdef _OPENMP
        omp_unset_lock(&lock);
#endif
    }
};

};  // namespace acclimate

#endif
