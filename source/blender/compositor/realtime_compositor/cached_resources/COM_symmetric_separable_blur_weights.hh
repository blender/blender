/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <memory>

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_cached_resource.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * Symmetric Separable Blur Weights Key.
 */
class SymmetricSeparableBlurWeightsKey {
 public:
  int type;
  float radius;

  SymmetricSeparableBlurWeightsKey(int type, float radius);

  uint64_t hash() const;
};

bool operator==(const SymmetricSeparableBlurWeightsKey &a,
                const SymmetricSeparableBlurWeightsKey &b);

/* -------------------------------------------------------------------------------------------------
 * Symmetric Separable Blur Weights.
 *
 * A cached resource that computes and caches a 1D GPU texture containing the weights of the
 * separable filter of the given type and radius. The filter is assumed to be symmetric, because
 * the filter functions are all even functions. Consequently, only the positive half of the filter
 * is computed and the shader takes that into consideration. */
class SymmetricSeparableBlurWeights : public CachedResource {
 private:
  GPUTexture *texture_ = nullptr;

 public:
  SymmetricSeparableBlurWeights(int type, float radius);

  ~SymmetricSeparableBlurWeights();

  void bind_as_texture(GPUShader *shader, const char *texture_name) const;

  void unbind_as_texture() const;
};

/* ------------------------------------------------------------------------------------------------
 * Symmetric Separable Blur Weights Container.
 */

class SymmetricSeparableBlurWeightsContainer : public CachedResourceContainer {
 private:
  Map<SymmetricSeparableBlurWeightsKey, std::unique_ptr<SymmetricSeparableBlurWeights>> map_;

 public:
  void reset() override;

  /* Check if there is an available SymmetricSeparableBlurWeights cached resource with the given
   * parameters in the container, if one exists, return it, otherwise, return a newly created one
   * and add it to the container. In both cases, tag the cached resource as needed to keep it
   * cached for the next evaluation. */
  SymmetricSeparableBlurWeights &get(int type, float radius);
};

}  // namespace blender::realtime_compositor
