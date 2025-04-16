/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float input_value = texture_load(input_tx, texel).x;
  float blurred_value = imageLoad(blurred_input_img, texel).x;

  imageStore(blurred_input_img, texel, float4(OPERATOR(input_value, blurred_value)));
}
