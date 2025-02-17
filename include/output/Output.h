// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

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
    non_owning_ptr<Model> model_;

  public:
    Output(Model* model) : model_(model) {}
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

    Model* model() { return model_; }
    const Model* model() const { return model_; }
    virtual std::string name() const { return "OUTPUT"; }
};
}  // namespace acclimate

#endif
