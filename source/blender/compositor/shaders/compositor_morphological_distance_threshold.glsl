/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_morphological_distance_threshold_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_morphological_distance_threshold)

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  const int2 texel = int2(gl_GlobalInvocationID.xy);

  const bool is_masked = texture_load(mask_tx, texel).x > 0.5f;
  const int2 closest_masked_texel = texture_load(flooded_masked_pixels_tx, texel).xy;
  const int2 closest_unmasked_texel = texture_load(flooded_unmasked_pixels_tx, texel).xy;
  const int2 closest_different_texel = is_masked ? closest_unmasked_texel : closest_masked_texel;
  const float distance_to_different = distance(float2(texel), float2(closest_different_texel));
  const float signed_distance = is_masked ? distance_to_different : -distance_to_different;
  const float value = clamp((signed_distance + distance_offset) / falloff_size, 0.0f, 1.0f);

  imageStore(output_img, texel, float4(value));
}
