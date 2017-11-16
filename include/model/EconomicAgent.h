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

#include "model/Storage.h"

namespace acclimate {

template<class ModelVariant>
class Region;
template<class ModelVariant>
class Firm;
template<class ModelVariant>
class Consumer;

template<class ModelVariant>
class EconomicAgent {
  public:
    enum class Type { CONSUMER, FIRM };

  private:
    typename ModelVariant::AgentParameters parameters_;

  protected:
    Forcing forcing_ = Forcing(1.0);

  public:
    Sector<ModelVariant>* const sector;
    Region<ModelVariant>* const region;
    std::vector<std::unique_ptr<Storage<ModelVariant>>> input_storages;
    const Type type;

  public:
    inline const typename ModelVariant::AgentParameters& parameters() const { return parameters_; };
    inline typename ModelVariant::AgentParameters const& parameters_writable() const {
        assertstep(INITIALIZATION);
        return parameters_;
    };

  protected:
    EconomicAgent(Sector<ModelVariant>* sector_p, Region<ModelVariant>* region_p, const EconomicAgent<ModelVariant>::Type& type_p);

  public:
    inline const Forcing& forcing() const { return forcing_; };
    inline void forcing(const Forcing& forcing_p) {
        assertstep(SCENARIO);
        assert(forcing_p >= 0.0);
        forcing_ = forcing_p;
    };
    virtual Firm<ModelVariant>* as_firm();
    virtual const Firm<ModelVariant>* as_firm() const;
    virtual Consumer<ModelVariant>* as_consumer();
    virtual const Consumer<ModelVariant>* as_consumer() const;
    inline bool is_firm() const { return type == Type::FIRM; };
    inline bool is_consumer() const { return type == Type::CONSUMER; };
    virtual ~EconomicAgent(){};
    virtual void iterate_consumption_and_production() = 0;
    virtual void iterate_expectation() = 0;
    virtual void iterate_purchase() = 0;
    virtual void iterate_investment() = 0;
    Storage<ModelVariant>* find_input_storage(const std::string& sector_name) const;
    void remove_storage(Storage<ModelVariant>* storage);
    virtual inline operator std::string() const { return std::string(*sector) + ":" + std::string(*region); }
#ifdef DEBUG
    virtual void print_details() const = 0;
#endif
};
}  // namespace acclimate

#endif
