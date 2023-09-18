/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  bool is_select = (nor.w > 0.0);
  bool is_hidden = (nor.w < 0.0);

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
  /* Add offset in Z to avoid Z-fighting and render selected wires on top. */
  /* TODO: scale this bias using Z-near and Z-far range. */
  gl_Position.z -= (is_select ? 2e-4 : 1e-4);

  if (is_hidden) {
    gl_Position = vec4(-2.0, -2.0, -2.0, 1.0);
  }

  finalColor = (is_select) ? vec4(1.0) : colorWire;
  finalColor.a = nor.w;

  gl_PointSize = sizeVertex * 2.0;

  view_clipping_distances(world_pos);
}
