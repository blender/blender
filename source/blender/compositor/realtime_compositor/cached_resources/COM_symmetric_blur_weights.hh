/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "BLI_math_vector_types.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_cached_resource.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * Symmetric Blur Weights Key.
 */
class SymmetricBlurWeightsKey {
 public:
  int type;
  float2 radius;

  SymmetricBlurWeightsKey(int type, float2 radius);

  uint64_t hash() const;
};

bool operator==(const SymmetricBlurWeightsKey &a, const SymmetricBlurWeightsKey &b);

/* -------------------------------------------------------------------------------------------------
 * Symmetric Blur Weights.
 *
 * A cached resource that computes and caches a 2D GPU texture containing the weights of the filter
 * of the given type and radius. The filter is assumed to be symmetric, because the filter
 * functions are evaluated on the normalized distance to the center. Consequently, only the upper
 * right quadrant are computed and the shader takes that into consideration. */
class SymmetricBlurWeights : public CachedResource {
 private:
  GPUTexture *texture_ = nullptr;

 public:
  SymmetricBlurWeights(int type, float2 radius);

  ~SymmetricBlurWeights();

  void bind_as_texture(GPUShader *shader, const char *texture_name) const;

  void unbind_as_texture() const;
};

}  // namespace blender::realtime_compositor
