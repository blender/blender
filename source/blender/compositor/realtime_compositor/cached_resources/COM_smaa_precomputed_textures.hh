/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_cached_resource.hh"

namespace blender::realtime_compositor {

/* -------------------------------------------------------------------------------------------------
 * SMAA Precomputed Textures.
 *
 * A cached resource that caches the precomputed textures needed by the SMAA algorithm. The
 * precomputed textures are constants, so this is a parameterless cached resource. */
class SMAAPrecomputedTextures : public CachedResource {
 private:
  GPUTexture *search_texture_ = nullptr;
  GPUTexture *area_texture_ = nullptr;

 public:
  SMAAPrecomputedTextures();

  ~SMAAPrecomputedTextures();

  void bind_search_texture(GPUShader *shader, const char *sampler_name) const;

  void unbind_search_texture() const;

  void bind_area_texture(GPUShader *shader, const char *sampler_name) const;

  void unbind_area_texture() const;
};

}  // namespace blender::realtime_compositor
