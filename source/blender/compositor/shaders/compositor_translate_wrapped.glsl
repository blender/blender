/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_bicubic_sampler_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  int2 size = texture_size(input_tx);
  float2 translated_coordinates = (float2(texel) + float2(0.5f) - translation) / float2(size);

  imageStore(output_img, texel, SAMPLER_FUNCTION(input_tx, translated_coordinates));
}
