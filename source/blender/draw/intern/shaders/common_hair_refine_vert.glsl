/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_hair_lib.glsl)

void main(void)
{
  float interp_time;
  vec4 data0, data1, data2, data3;
  hair_get_interp_attrs(data0, data1, data2, data3, interp_time);

  vec4 weights = hair_get_weights_cardinal(interp_time);
  finalColor = hair_interp_data(data0, data1, data2, data3, weights);

#if defined(TF_WORKAROUND)
  int id = gl_VertexID - idOffset;
  gl_Position.x = ((float(id % targetWidth) + 0.5) / float(targetWidth)) * 2.0 - 1.0;
  gl_Position.y = ((float(id / targetWidth) + 0.5) / float(targetHeight)) * 2.0 - 1.0;
  gl_Position.z = 0.0;
  gl_Position.w = 1.0;

  gl_PointSize = 1.0;
#else
#  ifdef GPU_METAL
  /* Metal still expects an output position for TF shaders. */
  gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
#  endif
#endif
}
