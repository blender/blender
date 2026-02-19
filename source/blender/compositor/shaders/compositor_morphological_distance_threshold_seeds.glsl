/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_morphological_distance_threshold_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_morphological_distance_threshold_seeds)

#include "gpu_shader_compositor_jump_flooding_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  const int2 texel = int2(gl_GlobalInvocationID.xy);

  const bool is_masked = texture_load(mask_tx, texel).x > 0.5f;

  const int2 masked_jump_flooding_value = initialize_jump_flooding_value(texel, is_masked);
  imageStore(masked_pixels_img, texel, int4(masked_jump_flooding_value, int2(0)));

  const int2 unmasked_jump_flooding_value = initialize_jump_flooding_value(texel, !is_masked);
  imageStore(unmasked_pixels_img, texel, int4(unmasked_jump_flooding_value, int2(0)));
}
