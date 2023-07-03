/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_reflection_probes.hh"
#include "eevee_instance.hh"

namespace blender::eevee {

void ReflectionProbeModule::init()
{
  if (!initialized_) {
    initialized_ = true;

    const int max_mipmap_levels = log(max_resolution_) + 1;
    probes_tx_.ensure_2d_array(GPU_RGBA16F,
                               int2(max_resolution_),
                               max_probes_,
                               GPU_TEXTURE_USAGE_SHADER_WRITE,
                               nullptr,
                               max_mipmap_levels);
    GPU_texture_mipmap_mode(probes_tx_, true, true);

    /* Cubemap is half of the resolution of the octahedral map. */
    cubemap_tx_.ensure_cube(
        GPU_RGBA16F, max_resolution_ / 2, GPU_TEXTURE_USAGE_ATTACHMENT, nullptr, 1);
    GPU_texture_mipmap_mode(cubemap_tx_, false, true);
  }

  {
    PassSimple &pass = remap_ps_;
    pass.init();
    pass.shader_set(instance_.shaders.static_shader_get(REFLECTION_PROBE_REMAP));
    pass.bind_texture("cubemap_tx", cubemap_tx_);
    pass.bind_image("octahedral_img", probes_tx_);
    pass.dispatch(int2(ceil_division(max_resolution_, REFLECTION_PROBE_GROUP_SIZE)));
  }
}

void ReflectionProbeModule::remap_to_octahedral_projection()
{
  instance_.manager->submit(remap_ps_);
  GPU_texture_update_mipmap_chain(probes_tx_);
}

}  // namespace blender::eevee
