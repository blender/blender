/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  /* Separate actual weight and alerts for independent interpolation */
  weight_interp = max(vec2(weight, -weight), 0.0);

  /* Saturate the weight to give a hint of the geometry behind the weights. */
#ifdef FAKE_SHADING
  vec3 view_normal = normalize(normal_object_to_view(nor));
  color_fac = abs(dot(view_normal, light_dir));
  color_fac = color_fac * 0.9 + 0.1;

#else
  color_fac = 1.0;
#endif

  view_clipping_distances(world_pos);
}
