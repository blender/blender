/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_cached_resource.hh"

namespace blender::realtime_compositor {

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
  GPUTexture *weights_texture_ = nullptr;
  GPUTexture *distance_falloffs_texture_ = nullptr;

 public:
  MorphologicalDistanceFeatherWeights(int type, int radius);

  ~MorphologicalDistanceFeatherWeights();

  void compute_weights(int radius);

  void compute_distance_falloffs(int type, int radius);

  void bind_weights_as_texture(GPUShader *shader, const char *texture_name) const;

  void unbind_weights_as_texture() const;

  void bind_distance_falloffs_as_texture(GPUShader *shader, const char *texture_name) const;

  void unbind_distance_falloffs_as_texture() const;
};

}  // namespace blender::realtime_compositor
