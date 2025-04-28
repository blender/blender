/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float radius = texture_load(radius_tx, texel).x;
  imageStore(radius_img, texel, float4(clamp(radius * scale, 0.0f, max_radius)));
}
