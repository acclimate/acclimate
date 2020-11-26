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
    static constexpr auto compression_level = 7;
    TimeStep flush_freq = 1;
    unsigned int event_cnt = 0;
    std::string filename;

    std::unique_ptr<netCDF::File> file;
    std::unique_ptr<netCDF::Variable> var_events;
    std::unique_ptr<netCDF::Variable> var_time;

    std::vector<netCDF::Variable> vars_model;
    std::vector<netCDF::Variable> vars_firms;
    std::vector<netCDF::Variable> vars_consumers;
    std::vector<netCDF::Variable> vars_sectors;
    std::vector<netCDF::Variable> vars_regions;
    std::vector<netCDF::Variable> vars_storages;
    std::vector<netCDF::Variable> vars_flows;

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
    NetCDFOutput(Model* model_p, const settings::SettingsNode& settings);
    void checkpoint_resume() override;
    void checkpoint_stop() override;
    void end() override;
    void iterate() override;
    void start() override;
};
}  // namespace acclimate

#endif
