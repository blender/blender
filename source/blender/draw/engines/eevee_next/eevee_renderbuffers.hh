/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * Render buffers are textures that are filled during a view rendering.
 * Their content is then added to the accumulation buffers of the film class.
 * They are short lived and can be reused when doing multi view rendering.
 */

#pragma once

#include "DRW_render.h"

#include "eevee_shader_shared.hh"

namespace blender::eevee {

class Instance;

class RenderBuffers {
 public:
  TextureFromPool depth_tx;
  TextureFromPool combined_tx;

  // TextureFromPool mist_tx; /* Derived from depth_tx during accumulation. */
  TextureFromPool normal_tx;
  TextureFromPool vector_tx;
  TextureFromPool diffuse_light_tx;
  TextureFromPool diffuse_color_tx;
  TextureFromPool specular_light_tx;
  TextureFromPool specular_color_tx;
  TextureFromPool volume_light_tx;
  TextureFromPool emission_tx;
  TextureFromPool environment_tx;
  TextureFromPool shadow_tx;
  TextureFromPool ambient_occlusion_tx;
  // TextureFromPool cryptomatte_tx; /* TODO */
  /* TODO(fclem): Use texture from pool once they support texture array. */
  Texture aov_color_tx;
  Texture aov_value_tx;

 private:
  Instance &inst_;

 public:
  RenderBuffers(Instance &inst) : inst_(inst){};

  void sync();
  /* Acquires (also ensures) the render buffer before rendering to them. */
  void acquire(int2 extent, void *owner);
  void release();
};

}  // namespace blender::eevee
