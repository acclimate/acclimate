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

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "acclimate.h"
#include "netcdftools.h"
#include "openmp.h"
#include "output/ArrayOutput.h"
#include "output/Output.h"

namespace settings {
class SettingsNode;
}  // namespace settings

struct tm;

namespace acclimate {

class Model;

class NetCDFOutput : public ArrayOutput {
  private:
    struct VariableMeta {
        std::vector<std::size_t> index;
        std::vector<std::size_t> sizes;
        netCDF::NcVar nc_var;
    };

  private:
    using ArrayOutput::include_events;
    using ArrayOutput::regions_size;
    using ArrayOutput::sectors_size;
    using ArrayOutput::stack;
    using ArrayOutput::variables;
    using Output::output_node;
    using Output::settings_string;
    static constexpr auto compression_level = 7;
    netCDF::NcDim dim_time;
    netCDF::NcDim dim_sector;
    netCDF::NcDim dim_region;
    std::unordered_map<hstring::hash_type, netCDF::NcGroup> groups;
    std::unique_ptr<netCDF::NcFile> file;
    netCDF::NcVar var_events;
    netCDF::NcVar var_time_variable;
    TimeStep flush_freq;
    unsigned int event_cnt;
    std::string filename;
    openmp::Lock netcdf_event_lock;
    std::string calendar;
    std::string time_units;

  private:
    void internal_write_header(tm* timestamp, unsigned int max_threads) override;
    void internal_write_footer(tm* duration) override;
    void internal_write_settings() override;
    void internal_iterate_begin() override;
    void internal_iterate_end() override;
    void internal_end() override;
    netCDF::NcGroup& create_group(const hstring& name);
    void create_variable_meta(typename ArrayOutput::Variable& v, const hstring& path, const hstring& name, const hstring& suffix) override;
    bool internal_handle_event(typename ArrayOutput::Event& event) override;

  public:
    NetCDFOutput(const settings::SettingsNode& settings_p, Model* model_p, settings::SettingsNode output_node_p);
    ~NetCDFOutput() override;
    void initialize() override;
    void flush() override;
    void checkpoint_stop() override;
    void checkpoint_resume() override;
    using Output::id;
    using Output::model;
};
}  // namespace acclimate

#endif
