/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_z_combine_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_z_combine_from_mask_depth)

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float first_z_value = texture_load(first_z_tx, texel).x;
  float second_z_value = texture_load(second_z_tx, texel).x;

  float combined_z = min(first_z_value, second_z_value);

  imageStore(combined_z_img, texel, float4(combined_z));
}
