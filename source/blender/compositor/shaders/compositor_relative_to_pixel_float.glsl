/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_relative_to_pixel_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_relative_to_pixel_float)

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  imageStore(output_img, texel, float4(texture_load(input_tx, texel).x * reference_size));
}
