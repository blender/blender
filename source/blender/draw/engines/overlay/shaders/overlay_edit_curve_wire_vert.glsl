/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 final_pos = pos;

  float flip = (gl_InstanceID != 0) ? -1.0 : 1.0;

  if (gl_VertexID % 2 == 0) {
    final_pos += normalSize * rad * (flip * nor - tan);
  }

  vec3 world_pos = point_object_to_world(final_pos);
  gl_Position = point_world_to_ndc(world_pos);

  finalColor = colorWireEdit;

  view_clipping_distances(world_pos);
}
