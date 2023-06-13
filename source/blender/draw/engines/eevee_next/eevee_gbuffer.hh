/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Gbuffer layout used for deferred shading pipeline.
 */

#pragma once

#include "DRW_render.h"

#include "eevee_material.hh"
#include "eevee_shader_shared.hh"

namespace blender::eevee {

class Instance;

/**
 * Full-screen textures containing geometric and surface data.
 * Used by deferred shading passes. Only one gbuffer is allocated per view
 * and is reused for each deferred layer. This is why there can only be temporary
 * texture inside it.
 *
 * Everything is stored inside two array texture, one for each format. This is to fit the
 * limitation of the number of images we can bind on a single shader.
 *
 * First layer is always for reflection. All parameters to shoot a reflection ray are inside
 * this layer.
 *
 * - Layer 1 : Reflection
 *   - R : Normal packed X
 *   - G : Normal packed Y
 *   - B : Roughness
 *   - A : Unused (Could be used for anisotropic roughness)
 *
 * Second layer is either for diffuse or transmission. Material mixing both are not
 * physically based and are uncommon. So in order to save bandwidth and texture memory, we only
 * store one. We use random sampling to mix between both. All parameters to shoot a refraction
 * ray are inside this layer.
 *
 * - Layer 2 : Refraction
 *   - R : Normal packed X
 *   - G : Normal packed Y
 *   - B : Roughness (isotropic)
 *   - A : IOR
 *
 * - Layer 2 : Diffuse / Sub-Surface Scattering
 *   - R : Normal packed X
 *   - G : Normal packed Y
 *   - B : Thickness
 *   - A : Unused (Could be used for diffuse roughness)
 *
 * Layer 3 is only allocated if Sub-Surface Scattering is needed. All parameters for
 * screen-space scattering are inside this layer.
 *
 * - Layer 3 : Sub-Surface Scattering
 *   - R : Scattering radius R
 *   - G : Scattering radius G
 *   - B : Scattering radius B
 *   - A : Object ID
 *
 * For each output closure, we also output the color to apply after the lighting computation.
 * The color is stored with a 2 exponent that allows input color with component higher than 1.
 * Color degradation is expected to happen in this case.
 */
struct GBuffer {
  /* TODO(fclem): Use texture from pool once they support texture array. */
  Texture closure_tx = {"GbufferClosure"};
  Texture color_tx = {"GbufferColor"};

  void acquire(int2 extent, eClosureBits closure_bits_)
  {
    const bool use_sss = (closure_bits_ & CLOSURE_SSS) != 0;
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE;
    closure_tx.ensure_2d_array(GPU_RGBA16, extent, use_sss ? 3 : 2, usage);
    color_tx.ensure_2d_array(GPU_RGB10_A2, extent, 2, usage);
  }

  void release()
  {
    /* TODO(fclem): Use texture from pool once they support texture array. */
    // closure_tx.release();
    // color_tx.release();
  }
};

}  // namespace blender::eevee
