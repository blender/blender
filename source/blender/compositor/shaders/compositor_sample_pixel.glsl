/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_sample_pixel_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_sample_pixel)

#include "gpu_shader_bicubic_sampler_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float4 sampled_color = SAMPLER_FUNCTION(input_tx, coordinates_u);

  imageStore(output_img, texel, sampled_color);
}
