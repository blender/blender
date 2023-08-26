/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_hair_lib.glsl)

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

#ifdef HAIR
  bool is_persp = (ProjectionMatrix[3][3] == 0.0);
  float time, thick_time, thickness;
  vec3 tan, binor;
  vec3 wpos;

  hair_get_pos_tan_binor_time(is_persp,
                              ModelMatrixInverse,
                              ViewMatrixInverse[3].xyz,
                              ViewMatrixInverse[2].xyz,
                              wpos,
                              tan,
                              binor,
                              time,
                              thickness,
                              thick_time);

  int id = hair_get_base_id();
  vec3 pos = texelFetch(hairPointBuffer, id).point_position;
  vec3 prv = texelFetch(prvBuffer, id).point_position;
  vec3 nxt = texelFetch(nxtBuffer, id).point_position;
#endif
  prevWorldPos = (prevModelMatrix * vec4(useDeform ? prv : pos, 1.0)).xyz;
  currWorldPos = (currModelMatrix * vec4(pos, 1.0)).xyz;
  nextWorldPos = (nextModelMatrix * vec4(useDeform ? nxt : pos, 1.0)).xyz;
  /* Use jittered projmatrix to be able to match exact sample depth (depth equal test).
   * Note that currModelMatrix needs to also be equal to ModelMatrix for the samples to match. */
#ifndef HAIR
  gl_Position = ProjectionMatrix * (ViewMatrix * vec4(currWorldPos, 1.0));
#else
  gl_Position = ProjectionMatrix * (ViewMatrix * vec4(wpos, 1.0));
#endif
}
