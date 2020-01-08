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

#ifndef ACCLIMATE_ECONOMICAGENT_H
#define ACCLIMATE_ECONOMICAGENT_H

#include <memory>
#include <string>
#include <vector>

#include "parameters.h"
#include "types.h"

namespace acclimate {

class Consumer;
class Firm;
class Model;
class Region;
class Sector;
class Storage;

class EconomicAgent {
  public:
    enum class Type { CONSUMER, FIRM };

  private:
    Parameters::AgentParameters parameters_;

  protected:
    Forcing forcing_ = Forcing(1.0);

  public:
    Sector* const sector;
    Region* const region;
    std::vector<std::unique_ptr<Storage>> input_storages;
    const Type type;

  public:
    inline const Parameters::AgentParameters& parameters() const { return parameters_; }
    Parameters::AgentParameters const& parameters_writable() const;

  protected:
    EconomicAgent(Sector* sector_p, Region* region_p, const EconomicAgent::Type& type_p);

  public:
    inline const Forcing& forcing() const { return forcing_; }

    void forcing(const Forcing& forcing_p);
    virtual Firm* as_firm();
    virtual const Firm* as_firm() const;
    virtual Consumer* as_consumer();
    virtual const Consumer* as_consumer() const;

    inline bool is_firm() const { return type == Type::FIRM; }
    inline bool is_consumer() const { return type == Type::CONSUMER; }

    virtual ~EconomicAgent() = default;
    virtual void iterate_consumption_and_production() = 0;
    virtual void iterate_expectation() = 0;
    virtual void iterate_purchase() = 0;
    virtual void iterate_investment() = 0;
    Storage* find_input_storage(const std::string& sector_name) const;
    void remove_storage(Storage* storage);
    Model* model() const;
    virtual std::string id() const;
    // DEBUG
    virtual void print_details() const = 0;
};
}  // namespace acclimate

#endif
