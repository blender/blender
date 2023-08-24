/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

vec3 debug_random_color(int v)
{
  float r = interlieved_gradient_noise(vec2(v, 0), 0.0, 0.0);
  return hue_gradient(r);
}

void main()
{
  Surfel surfel = surfels_buf[surfel_index];

  vec4 radiance_vis = vec4(0.0);
  radiance_vis += gl_FrontFacing ? surfel.radiance_direct.front : surfel.radiance_direct.back;
  radiance_vis += gl_FrontFacing ? surfel.radiance_indirect[1].front :
                                   surfel.radiance_indirect[1].back;

  switch (eDebugMode(debug_mode)) {
    default:
    case DEBUG_IRRADIANCE_CACHE_SURFELS_NORMAL:
      out_color = vec4(pow(surfel.normal * 0.5 + 0.5, vec3(2.2)), 0.0);
      break;
    case DEBUG_IRRADIANCE_CACHE_SURFELS_CLUSTER:
      out_color = vec4(pow(debug_random_color(surfel.cluster_id), vec3(2.2)), 0.0);
      break;
    case DEBUG_IRRADIANCE_CACHE_SURFELS_IRRADIANCE:
      out_color = vec4(radiance_vis.rgb, 0.0);
      break;
    case DEBUG_IRRADIANCE_CACHE_SURFELS_VISIBILITY:
      out_color = vec4(radiance_vis.aaa, 0.0);
      break;
  }

  /* Display surfels as circles. */
  if (distance(P, surfel.position) > surfel_radius) {
    discard;
    return;
  }
}
