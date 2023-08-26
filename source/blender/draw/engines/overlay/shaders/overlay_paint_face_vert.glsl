/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

#ifdef GPU_METAL
  /* Small bias to always be on top of the geom. */
  gl_Position.z -= 5e-5;
#endif

  bool is_select = (nor.w > 0.0);
  bool is_hidden = (nor.w < 0.0);

  /* Don't draw faces that are selected. */
  if (is_hidden || is_select) {
    gl_Position = vec4(-2.0, -2.0, -2.0, 1.0);
  }
  else {
    view_clipping_distances(world_pos);
  }
}
