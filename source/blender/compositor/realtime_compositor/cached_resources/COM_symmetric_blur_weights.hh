/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <memory>

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_cached_resource.hh"

namespace blender::realtime_compositor {

class Context;

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
  SymmetricBlurWeights(Context &context, int type, float2 radius);

  ~SymmetricBlurWeights();

  void bind_as_texture(GPUShader *shader, const char *texture_name) const;

  void unbind_as_texture() const;
};

/* ------------------------------------------------------------------------------------------------
 * Symmetric Blur Weights Container.
 */
class SymmetricBlurWeightsContainer : public CachedResourceContainer {
 private:
  Map<SymmetricBlurWeightsKey, std::unique_ptr<SymmetricBlurWeights>> map_;

 public:
  void reset() override;

  /* Check if there is an available SymmetricBlurWeights cached resource with the given parameters
   * in the container, if one exists, return it, otherwise, return a newly created one and add it
   * to the container. In both cases, tag the cached resource as needed to keep it cached for the
   * next evaluation. */
  SymmetricBlurWeights &get(Context &context, int type, float2 radius);
};

}  // namespace blender::realtime_compositor
