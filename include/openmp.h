// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_OPENMP_H
#define ACCLIMATE_OPENMP_H

#ifdef _OPENMP
#include <omp.h>
#endif

namespace acclimate::openmp {

class Lock {
  protected:
#ifdef _OPENMP
    omp_lock_t lock = {};
#endif
  public:
    Lock() {
#ifdef _OPENMP
        omp_init_lock(&lock);
#endif
    }

    ~Lock() {
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

inline unsigned int get_thread_count() {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

}  // namespace acclimate::openmp

#endif
