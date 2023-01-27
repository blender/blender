/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"

#include "COM_morphological_distance_feather_weights.hh"
#include "COM_symmetric_blur_weights.hh"
#include "COM_symmetric_separable_blur_weights.hh"

namespace blender::realtime_compositor {

/* -------------------------------------------------------------------------------------------------
 * Static Cache Manager
 *
 * A static cache manager is a collection of cached resources that can be retrieved when needed and
 * created if not already available. In particular, each cached resource type has its own Map in
 * the class, where all instances of that cached resource type are stored and tracked. See the
 * CachedResource class for more information.
 *
 * The manager deletes the cached resources that are no longer needed. A cached resource is said to
 * be not needed when it was not used in the previous evaluation. This is done through the
 * following mechanism:
 *
 * - Before every evaluation, do the following:
 *     1. All resources whose CachedResource::needed flag is false are deleted.
 *     2. The CachedResource::needed flag of all remaining resources is set to false.
 * - During evaluation, when retrieving any cached resource, set its CachedResource::needed flag to
 *   true.
 *
 * In effect, any resource that was used in the previous evaluation but was not used in the current
 * evaluation will be deleted before the next evaluation. This mechanism is implemented in the
 * reset() method of the class, which should be called before every evaluation. */
class StaticCacheManager {
 private:
  /* A map that stores all SymmetricBlurWeights cached resources. */
  Map<SymmetricBlurWeightsKey, std::unique_ptr<SymmetricBlurWeights>> symmetric_blur_weights_;

  /* A map that stores all SymmetricSeparableBlurWeights cached resources. */
  Map<SymmetricSeparableBlurWeightsKey, std::unique_ptr<SymmetricSeparableBlurWeights>>
      symmetric_separable_blur_weights_;

  /* A map that stores all MorphologicalDistanceFeatherWeights cached resources. */
  Map<MorphologicalDistanceFeatherWeightsKey, std::unique_ptr<MorphologicalDistanceFeatherWeights>>
      morphological_distance_feather_weights_;

 public:
  /* Reset the cache manager by deleting the cached resources that are no longer needed because
   * they weren't used in the last evaluation and prepare the remaining cached resources to track
   * their needed status in the next evaluation. See the class description for more information.
   * This should be called before every evaluation. */
  void reset();

  /* Check if there is an available SymmetricBlurWeights cached resource with the given parameters
   * in the manager, if one exists, return it, otherwise, return a newly created one and add it to
   * the manager. In both cases, tag the cached resource as needed to keep it cached for the next
   * evaluation. */
  SymmetricBlurWeights &get_symmetric_blur_weights(int type, float2 radius);

  /* Check if there is an available SymmetricSeparableBlurWeights cached resource with the given
   * parameters in the manager, if one exists, return it, otherwise, return a newly created one and
   * add it to the manager. In both cases, tag the cached resource as needed to keep it cached for
   * the next evaluation. */
  SymmetricSeparableBlurWeights &get_symmetric_separable_blur_weights(int type, float radius);

  /* Check if there is an available MorphologicalDistanceFeatherWeights cached resource with the
   * given parameters in the manager, if one exists, return it, otherwise, return a newly created
   * one and add it to the manager. In both cases, tag the cached resource as needed to keep it
   * cached for the next evaluation. */
  MorphologicalDistanceFeatherWeights &get_morphological_distance_feather_weights(int type,
                                                                                  int radius);
};

}  // namespace blender::realtime_compositor
