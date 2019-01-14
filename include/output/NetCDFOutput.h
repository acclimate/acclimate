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
#include "netcdf_headers.h"
#include "output/ArrayOutput.h"
#include "types.h"

namespace acclimate {

template<class ModelVariant>
class Model;
template<class ModelVariant>
class Scenario;

template<class ModelVariant>
class NetCDFOutput : public ArrayOutput<ModelVariant> {
  public:
    using Output<ModelVariant>::id;
    using Output<ModelVariant>::model;
    using Output<ModelVariant>::output_node;
    using Output<ModelVariant>::settings;
    using Output<ModelVariant>::scenario;

  protected:
    struct VariableMeta {
        std::vector<std::size_t> index;
        std::vector<std::size_t> sizes;
        netCDF::NcVar nc_var;
    };
    using ArrayOutput<ModelVariant>::regions_size;
    using ArrayOutput<ModelVariant>::sectors_size;
    using ArrayOutput<ModelVariant>::variables;
    using ArrayOutput<ModelVariant>::stack;
    using ArrayOutput<ModelVariant>::include_events;
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
    OpenMPLock netcdf_event_lock;

  protected:
    void internal_write_header(tm* timestamp, int max_threads) override;
    void internal_write_footer(tm* duration) override;
    void internal_write_settings() override;
    void internal_iterate_begin() override;
    void internal_iterate_end() override;
    void internal_start() override;
    void internal_end() override;
    netCDF::NcGroup& create_group(const hstring& name);
    void create_variable_meta(typename ArrayOutput<ModelVariant>::Variable& v, const hstring& path, const hstring& name, const hstring& suffix) override;
    bool internal_handle_event(typename ArrayOutput<ModelVariant>::Event& event) override;

  public:
    NetCDFOutput(const settings::SettingsNode& settings_p,
                 Model<ModelVariant>* model_p,
                 Scenario<ModelVariant>* scenario_p,
                 settings::SettingsNode output_node_p);
    ~NetCDFOutput();
    void initialize() override;
    void flush() override;
    void checkpoint_stop() override;
    void checkpoint_resume() override;
};
}  // namespace acclimate

#endif
