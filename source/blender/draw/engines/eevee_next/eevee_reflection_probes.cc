/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_reflection_probes.hh"
#include "eevee_instance.hh"

namespace blender::eevee {

void ReflectionProbeModule::init()
{
  if (!initialized_) {
    const int max_mipmap_levels = log(max_resolution_) + 1;
    cubemaps_tx_.ensure_cube_array(GPU_RGBA16F,
                                   max_resolution_,
                                   max_probes_,
                                   GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT,
                                   nullptr,
                                   max_mipmap_levels);
    GPU_texture_mipmap_mode(cubemaps_tx_, true, true);
    initialized_ = true;
  }
}

}  // namespace blender::eevee
