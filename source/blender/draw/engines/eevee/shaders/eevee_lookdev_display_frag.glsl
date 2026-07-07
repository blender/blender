/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_lookdev_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_lookdev_display)

void main()
{
  float2 texture_size = float2(textureSize(metallic_tx, 0));
  float2 texel_size = float2(1.0f) / texture_size;

  float distance_from_center = distance(uv_coord.xy, float2(0.5f));
  if (distance_from_center > 0.5f) {
    gpu_discard_fragment();
    return;
  }
  float smooth_size = texel_size.x * 1.5f;
  float smooth_offset = texel_size.x * 0.5f;
  float factor = (distance_from_center - 0.5f + smooth_offset + smooth_size) *
                 (1.0f / smooth_size);
  float alpha = clamp(1.0f - factor, 0.0f, 1.0f);

  float4 color = sphere_id == 0 ? texture(metallic_tx, uv_coord.xy, 0) :
                                  texture(diffuse_tx, uv_coord.xy, 0);
  color.a = alpha;
  out_color = color;

  /* Ensure balls are on top of overlays. */
  gl_FragDepth = 0.0f;
}
