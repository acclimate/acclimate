/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
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

#ifndef ACCLIMATE_OUTPUT_H
#define ACCLIMATE_OUTPUT_H

#include <string>

#include "acclimate.h"

namespace acclimate {

class EconomicAgent;
enum class EventType : unsigned char;
class Model;
class Sector;

class Output {
  protected:
    non_owning_ptr<Model> model_m;

  public:
    Output(Model* model_p) : model_m(model_p) {}
    virtual ~Output() = default;
    virtual void checkpoint_resume() {}
    virtual void checkpoint_stop() {}
    virtual void end() {}
    virtual void event(EventType /* type */, const EconomicAgent* /* economic_agent */, FloatType /* value */) {}
    virtual void event(EventType /* type */,
                       const EconomicAgent* /* economic_agent_from */,
                       const EconomicAgent* /* economic_agent_to */,
                       FloatType /* value */) {}
    virtual void event(EventType /* type */, const Sector* /* sector */, const EconomicAgent* /* economic_agent */, FloatType /* value */) {}
    virtual void iterate() {}
    virtual void start() {}

    Model* model() { return model_m; }
    const Model* model() const { return model_m; }
    virtual std::string name() const { return "OUTPUT"; }
};
}  // namespace acclimate

#endif
