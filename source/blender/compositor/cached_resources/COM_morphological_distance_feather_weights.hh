/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <memory>

#include "BLI_array.hh"
#include "BLI_map.hh"

#include "COM_cached_resource.hh"

namespace blender::realtime_compositor {

class Context;

/* ------------------------------------------------------------------------------------------------
 * Morphological Distance Feather Key.
 */
class MorphologicalDistanceFeatherWeightsKey {
 public:
  int type;
  float radius;

  MorphologicalDistanceFeatherWeightsKey(int type, float radius);

  uint64_t hash() const;
};

bool operator==(const MorphologicalDistanceFeatherWeightsKey &a,
                const MorphologicalDistanceFeatherWeightsKey &b);

/* -------------------------------------------------------------------------------------------------
 * Morphological Distance Feather Weights.
 *
 * A cached resource that computes and caches 1D GPU textures containing the weights of the
 * separable Gaussian filter of the given radius as well as an inverse distance falloff of the
 * given type and radius. The weights and falloffs are symmetric, because the Gaussian and falloff
 * functions are all even functions. Consequently, only the positive half of the filter is computed
 * and the shader takes that into consideration. */
class MorphologicalDistanceFeatherWeights : public CachedResource {
 private:
  Array<float> weights_;
  Array<float> falloffs_;

 public:
  Result weights_result;
  Result falloffs_result;

 public:
  MorphologicalDistanceFeatherWeights(Context &context, int type, int radius);

  ~MorphologicalDistanceFeatherWeights();

 private:
  void compute_weights(int radius);

  void compute_distance_falloffs(int type, int radius);
};

/* ------------------------------------------------------------------------------------------------
 * Morphological Distance Feather Key.
 */
class MorphologicalDistanceFeatherWeightsContainer : CachedResourceContainer {
 private:
  Map<MorphologicalDistanceFeatherWeightsKey, std::unique_ptr<MorphologicalDistanceFeatherWeights>>
      map_;

 public:
  void reset() override;

  /* Check if there is an available MorphologicalDistanceFeatherWeights cached resource with the
   * given parameters in the container, if one exists, return it, otherwise, return a newly created
   * one and add it to the container. In both cases, tag the cached resource as needed to keep it
   * cached for the next evaluation. */
  MorphologicalDistanceFeatherWeights &get(Context &context, int type, int radius);
};

}  // namespace blender::realtime_compositor
