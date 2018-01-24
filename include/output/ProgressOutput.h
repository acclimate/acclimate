#ifndef ACCLIMATE_PROGRESSOUTPUT_H
#define ACCLIMATE_PROGRESSOUTPUT_H

#include <output/Output.h>

namespace tqdm {
template<typename SizeType>
class RangeIterator;
template<typename IteratorType>
class Tqdm;
template<typename SizeType = int>
using RangeTqdm = Tqdm<RangeIterator<SizeType>>;
}  // namespace tqdm

namespace acclimate {

template<class ModelVariant>
class Model;
template<class ModelVariant>
class Sector;
template<class ModelVariant>
class Scenario;

template<class ModelVariant>
class ProgressOutput : public Output<ModelVariant> {
  public:
    using Output<ModelVariant>::output_node;
    using Output<ModelVariant>::model;
    using Output<ModelVariant>::settings;

  private:
#ifdef USE_TQDM
    std::unique_ptr<tqdm::RangeTqdm<int>> it;
#endif

  protected:
    void internal_iterate_end() override;
    void internal_end() override;

  public:
    ProgressOutput(const settings::SettingsNode& settings,
                   Model<ModelVariant>* model,
                   Scenario<ModelVariant>* scenario,
                   const settings::SettingsNode& output_node);
    void initialize() override;
};
}  // namespace acclimate

#endif
