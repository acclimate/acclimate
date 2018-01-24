#include "output/ProgressOutput.h"
#include <iostream>
#include "model/Model.h"
#include "model/Sector.h"
#include "scenario/Scenario.h"
#include "variants/ModelVariants.h"
#ifdef USE_TQDM
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#include "tqdm/tqdm.h"
#pragma GCC diagnostic pop
#endif

namespace acclimate {

template<class ModelVariant>
ProgressOutput<ModelVariant>::ProgressOutput(const settings::SettingsNode& settings_p,
                                             Model<ModelVariant>* model_p,
                                             Scenario<ModelVariant>* scenario_p,
                                             const settings::SettingsNode& output_node_p)
    : Output<ModelVariant>(settings_p, model_p, scenario_p, output_node_p) {}

template<class ModelVariant>
void ProgressOutput<ModelVariant>::initialize() {
#ifdef USE_TQDM
    tqdm::Params p;
    p.ascii = "";
    p.f = stdout;
    const int total = output_node["total"].template as<int>();
    it.reset(new tqdm::RangeTqdm<int>(tqdm::RangeIterator<int>(total), tqdm::RangeIterator<int>(total, total), p));
#else
    error("tqdm not enabled");
#endif
}

template<class ModelVariant>
void ProgressOutput<ModelVariant>::internal_end() {
#ifdef USE_TQDM
    it->close();
#endif
}

template<class ModelVariant>
void ProgressOutput<ModelVariant>::internal_iterate_end() {
#ifdef USE_TQDM
    if (!it->ended()) {
        ++(*it);
    }
#endif
}

INSTANTIATE_BASIC(ProgressOutput);
}  // namespace acclimate
