/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  vec4 color = texture_load(input_tx, texel);
  float alpha = color.a > 0.0 ? color.a : 1.0;
  vec3 corrected_color = FUNCTION(max(color.rgb / alpha, vec3(0.0))) * alpha;
  imageStore(output_img, texel, vec4(corrected_color, color.a));
}
