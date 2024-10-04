/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  vec4 input_color = texture_load(input_tx, texel);
  imageStore(output_img, texel, input_color * vec4(vec3(input_color.a), 1.0));
}
