/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Step 4 : Apply final integration on top of the scene color. */

#include "infos/eevee_volume_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_volume_resolve)

#include "eevee_volume_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

void main()
{
  float2 uvs = gl_FragCoord.xy * uniform_buf.volumes.main_view_extent_inv;
  float scene_depth = texelFetch(hiz_tx, int2(gl_FragCoord.xy), 0).r;

  VolumeResolveSample vol = volume_resolve(
      float3(uvs, scene_depth), volume_transmittance_tx, volume_scattering_tx);

  out_radiance = float4(vol.scattering, 0.0f);
  out_transmittance = float4(vol.transmittance, saturate(average(vol.transmittance)));

  if (uniform_buf.render_pass.volume_light_id >= 0) {
    imageStoreFast(rp_color_img,
                   int3(int2(gl_FragCoord.xy), uniform_buf.render_pass.volume_light_id),
                   float4(vol.scattering, 1.0f));
  }
}
