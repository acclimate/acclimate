// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_EXCEPTIONS_H
#define ACCLIMATE_EXCEPTIONS_H

#include <stdexcept>
#include <string>

// IWYU pragma: private, include "acclimate.h"

namespace acclimate {

class exception : public std::runtime_error {
  public:
    explicit exception(const std::string& s) : std::runtime_error(s) {}

    explicit exception(const char* s) : std::runtime_error(s) {}
};

class return_after_checkpoint : public std::exception {};

}  // namespace acclimate

#endif
