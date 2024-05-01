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
 * Bokeh Kernel Key.
 */
class BokehKernelKey {
 public:
  int2 size;
  int sides;
  float rotation;
  float roundness;
  float catadioptric;
  float lens_shift;

  BokehKernelKey(
      int2 size, int sides, float rotation, float roundness, float catadioptric, float lens_shift);

  uint64_t hash() const;
};

bool operator==(const BokehKernelKey &a, const BokehKernelKey &b);

/* -------------------------------------------------------------------------------------------------
 * Bokeh Kernel.
 *
 * A cached resource that computes and caches a GPU texture containing the unnormalized convolution
 * kernel, which when convolved with an image emulates a bokeh lens with the given parameters. */
class BokehKernel : public CachedResource {
 private:
  GPUTexture *texture_ = nullptr;

 public:
  BokehKernel(Context &context,
              int2 size,
              int sides,
              float rotation,
              float roundness,
              float catadioptric,
              float lens_shift);

  ~BokehKernel();

  void bind_as_texture(GPUShader *shader, const char *texture_name) const;

  void unbind_as_texture() const;

  GPUTexture *texture() const;
};

/* ------------------------------------------------------------------------------------------------
 * Bokeh Kernel Container.
 */
class BokehKernelContainer : CachedResourceContainer {
 private:
  Map<BokehKernelKey, std::unique_ptr<BokehKernel>> map_;

 public:
  void reset() override;

  /* Check if there is an available BokehKernel cached resource with the given parameters in the
   * container, if one exists, return it, otherwise, return a newly created one and add it to the
   * container. In both cases, tag the cached resource as needed to keep it cached for the next
   * evaluation. */
  BokehKernel &get(Context &context,
                   int2 size,
                   int sides,
                   float rotation,
                   float roundness,
                   float catadioptric,
                   float lens_shift);
};

}  // namespace blender::realtime_compositor
