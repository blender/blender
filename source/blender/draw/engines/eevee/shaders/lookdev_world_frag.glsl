/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(lightprobe_lib.glsl)
#pragma BLENDER_REQUIRE(surface_lib.glsl)

vec3 background_transform_to_world(vec3 viewvec)
{
  vec4 v = (ProjectionMatrix[3][3] == 0.0) ? vec4(viewvec, 1.0) : vec4(0.0, 0.0, 1.0, 1.0);
  vec4 co_homogeneous = (ProjectionMatrixInverse * v);

  vec4 co = vec4(co_homogeneous.xyz / co_homogeneous.w, 0.0);
  return (ViewMatrixInverse * co).xyz;
}

float hypot(float x, float y)
{
  return sqrt(x * x + y * y);
}

vec4 node_tex_environment_equirectangular(vec3 co, sampler2D ima)
{
  vec3 nco = normalize(co);
  float u = -atan(nco.y, nco.x) / (2.0 * M_PI) + 0.5;
  float v = atan(nco.z, hypot(nco.x, nco.y)) / M_PI + 0.5;
  return textureLod(ima, vec2(u, v), 0.0);
}

void main()
{
  vec3 worldvec = background_transform_to_world(viewPosition);

  vec3 background_color;
#if defined(LOOKDEV_BG)
  background_color = probe_evaluate_world_spec(worldvec, studioLightBlur).rgb;
#else
  worldvec = StudioLightMatrix * worldvec;
  background_color = node_tex_environment_equirectangular(worldvec, studioLight).rgb;
  background_color *= studioLightIntensity;
#endif

  FragColor = vec4(clamp(background_color, vec3(0.0), vec3(1e10)), 1.0) * backgroundAlpha;
}
