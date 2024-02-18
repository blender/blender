/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  float radius = texture_load(radius_tx, texel).x;
  imageStore(radius_img, texel, vec4(clamp(radius * scale, 0.0, max_radius)));
}
