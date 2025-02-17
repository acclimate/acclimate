// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_NETCDFOUTPUT_H
#define ACCLIMATE_NETCDFOUTPUT_H

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "acclimate.h"
#include "output/ArrayOutput.h"

namespace netCDF {
class Dimension;  // IWYU pragma: keep
class File;       // IWYU pragma: keep
class Variable;   // IWYU pragma: keep
}  // namespace netCDF

namespace settings {
class SettingsNode;
}  // namespace settings

namespace acclimate {

class Model;

class NetCDFOutput final : public ArrayOutput {
  private:
    static constexpr auto compression_level_ = 7;
    TimeStep flush_freq_ = 1;
    unsigned int event_cnt_ = 0;
    std::string filename_;

    std::unique_ptr<netCDF::File> file_;
    std::unique_ptr<netCDF::Variable> var_events_;
    std::unique_ptr<netCDF::Variable> var_time_;

    std::vector<netCDF::Variable> vars_model_;
    std::vector<netCDF::Variable> vars_firms_;
    std::vector<netCDF::Variable> vars_consumers_;
    std::vector<netCDF::Variable> vars_sectors_;
    std::vector<netCDF::Variable> vars_regions_;
    std::vector<netCDF::Variable> vars_locations_;
    std::vector<netCDF::Variable> vars_storages_;
    std::vector<netCDF::Variable> vars_flows_;

  private:
    template<std::size_t dim>
    void write_variables(const Observable<dim>& observable, std::vector<netCDF::Variable>& nc_variables);

    template<std::size_t dim>
    void create_group(const char* name,
                      const std::array<netCDF::Dimension, dim + 1>& default_dims,
                      const std::array<const char*, dim>& index_names,
                      const Observable<dim>& observable,
                      std::vector<netCDF::Variable>& nc_variables);

  public:
    NetCDFOutput(Model* model, const settings::SettingsNode& settings);
    void checkpoint_resume() override;
    void checkpoint_stop() override;
    void end() override;
    void iterate() override;
    void start() override;
};
}  // namespace acclimate

#endif
