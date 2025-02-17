// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_EXTERNALFORCING_H
#define ACCLIMATE_EXTERNALFORCING_H

#include <memory>
#include <string>

#include "acclimate.h"
#include "netcdfpp.h"

namespace acclimate {

class ExternalForcing {
  protected:
    TimeStep time_index_;
    TimeStep time_index_count_;
    netCDF::File file_;
    std::unique_ptr<netCDF::Variable> variable_;
    std::unique_ptr<netCDF::Variable> time_variable_;

  protected:
    virtual void read_data() = 0;

  public:
    ExternalForcing(std::string filename, std::string variable_name);
    virtual ~ExternalForcing();
    int next_timestep();
    std::string calendar_str() const;
    std::string time_units_str() const;
};
}  // namespace acclimate

#endif
