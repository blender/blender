/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_cavity_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_curvature_lib.glsl)

void main()
{
  float cavity = 0.0, edges = 0.0, curvature = 0.0;

#ifdef USE_CAVITY
  cavity_compute(uvcoordsvar.xy, depthBuffer, normalBuffer, cavity, edges);
#endif

#ifdef USE_CURVATURE
  curvature_compute(uvcoordsvar.xy, objectIdBuffer, normalBuffer, curvature);
#endif

  float final_cavity_factor = clamp((1.0 - cavity) * (1.0 + edges) * (1.0 + curvature), 0.0, 4.0);

  fragColor.rgb = vec3(final_cavity_factor);
  fragColor.a = 1.0;
}
