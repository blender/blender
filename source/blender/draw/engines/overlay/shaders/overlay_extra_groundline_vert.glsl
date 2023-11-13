/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  finalColor = colorLight;

  /* Relative to DPI scaling. Have constant screen size. */
  vec3 screen_pos = ViewMatrixInverse[0].xyz * pos.x + ViewMatrixInverse[1].xyz * pos.y;
  vec3 p = inst_pos;
  p.z *= (pos.z == 0.0) ? 0.0 : 1.0;
  float screen_size = mul_project_m4_v3_zfac(p) * sizePixel;
  vec3 world_pos = p + screen_pos * screen_size;

  gl_Position = point_world_to_ndc(world_pos);

  /* Convert to screen position [0..sizeVp]. */
  edgePos = edgeStart = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;

  view_clipping_distances(world_pos);
}
