/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_bicubic_sampler_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  ivec2 size = texture_size(input_tx);
  vec2 translated_coordinates = (vec2(texel) + vec2(0.5f) - translation) / vec2(size);

  imageStore(output_img, texel, SAMPLER_FUNCTION(input_tx, translated_coordinates));
}
