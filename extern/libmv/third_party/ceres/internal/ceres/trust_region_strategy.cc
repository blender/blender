#include "ceres/trust_region_strategy.h"
#include "ceres/dogleg_strategy.h"
#include "ceres/levenberg_marquardt_strategy.h"

namespace ceres {
namespace internal {

TrustRegionStrategy::~TrustRegionStrategy() {}

TrustRegionStrategy* TrustRegionStrategy::Create(const Options& options) {
  switch (options.trust_region_strategy_type) {
    case LEVENBERG_MARQUARDT:
      return new LevenbergMarquardtStrategy(options);
    case DOGLEG:
      return new DoglegStrategy(options);
    default:
      LOG(FATAL) << "Unknown trust region strategy: "
                 << options.trust_region_strategy_type;
  }

  LOG(FATAL) << "Unknown trust region strategy: "
             << options.trust_region_strategy_type;
  return NULL;
}

}  // namespace internal
}  // namespace ceres
