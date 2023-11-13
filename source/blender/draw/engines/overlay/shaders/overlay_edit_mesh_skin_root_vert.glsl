/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  mat3 imat = mat3(ModelMatrixInverse);
  vec3 right = normalize(imat * ViewMatrixInverse[0].xyz);
  vec3 up = normalize(imat * ViewMatrixInverse[1].xyz);
  vec3 screen_pos = (right * pos.x + up * pos.z) * size;
  vec4 pos_4d = ModelMatrix * vec4(local_pos + screen_pos, 1.0);
  gl_Position = drw_view.winmat * (drw_view.viewmat * pos_4d);
  /* Manual stipple: one segment out of 2 is transparent. */
  finalColor = ((gl_VertexID & 1) == 0) ? colorSkinRoot : vec4(0.0);

  view_clipping_distances(pos_4d.xyz);
}
