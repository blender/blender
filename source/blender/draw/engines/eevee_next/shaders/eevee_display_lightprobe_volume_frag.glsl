/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(draw_view_lib.glsl)

void main()
{
  float dist_sqr = dot(lP, lP);

  /* Discard outside the circle. */
  if (dist_sqr > 1.0) {
    discard;
    return;
  }

  SphericalHarmonicL1 sh;
  sh.L0.M0 = texelFetch(irradiance_a_tx, cell, 0);
  sh.L1.Mn1 = texelFetch(irradiance_b_tx, cell, 0);
  sh.L1.M0 = texelFetch(irradiance_c_tx, cell, 0);
  sh.L1.Mp1 = texelFetch(irradiance_d_tx, cell, 0);
  float validity = texelFetch(validity_tx, cell, 0).r;

  vec3 vN = vec3(lP, sqrt(max(0.0, 1.0 - dist_sqr)));
  vec3 N = drw_normal_view_to_world(vN);
  vec3 lN = transform_direction(world_to_grid, N);

  vec3 irradiance = spherical_harmonics_evaluate_lambert(lN, sh);

  if (display_validity) {
    out_color = vec4(mix(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), validity), 0.0);
  }
  else {
    out_color = vec4(irradiance, 0.0);
  }
}
