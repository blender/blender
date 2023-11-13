/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  const float sphere_radius = 0.05;

  bool is_perp = (drw_view.winmat[3][3] == 0.0);
  vec3 ray_ori_view = (is_perp) ? vec3(0.0) : viewPosition.xyz;
  vec3 ray_dir_view = (is_perp) ? viewPosition : vec3(0.0, 0.0, -1.0);

  /* Single matrix mul without branch. */
  vec4 mul_vec = (is_perp) ? vec4(ray_dir_view, 0.0) : vec4(ray_ori_view, 1.0);
  vec3 mul_res = (sphereMatrix * mul_vec).xyz;

  /* Reminder :
   * sphereMatrix[3] is the view space origin in sphere space (sph_ori -> view_ori).
   * sphereMatrix[2] is the view space Z axis in sphere space. */

  /* convert to sphere local space */
  vec3 ray_ori = (is_perp) ? sphereMatrix[3].xyz : mul_res;
  vec3 ray_dir = (is_perp) ? mul_res : -sphereMatrix[2].xyz;
  float ray_len = length(ray_dir);
  ray_dir /= ray_len;

  /* Line to sphere intersect */
  const float sphere_radius_sqr = sphere_radius * sphere_radius;
  float b = dot(ray_ori, ray_dir);
  float c = dot(ray_ori, ray_ori) - sphere_radius_sqr;
  float h = b * b - c;
  float t = -sqrt(max(0.0, h)) - b;

  /* Compute dot product for lighting */
  vec3 p = ray_dir * t + ray_ori; /* Point on sphere */
  vec3 n = normalize(p);          /* Normal is just the point in sphere space, normalized. */
  vec3 l = normalize(sphereMatrix[2].xyz); /* Just the view Z axis in the sphere space. */

  /* Smooth lighting factor. */
  const float s = 0.2; /* [0.0-0.5] range */
  float fac = clamp((dot(n, l) * (1.0 - s)) + s, 0.0, 1.0);
  fragColor.rgb = mix(finalStateColor, finalBoneColor, fac * fac);

  /* 2x2 dither pattern to smooth the lighting. */
  float dither = (0.5 + dot(vec2(ivec2(gl_FragCoord.xy) & ivec2(1)), vec2(1.0, 2.0))) * 0.25;
  dither *= (1.0 / 255.0); /* Assume 8bit per color buffer. */

  fragColor = vec4(fragColor.rgb + dither, alpha);
  lineOutput = vec4(0.0);

  t /= ray_len;
  gl_FragDepth = get_depth_from_view_z(ray_dir_view.z * t + ray_ori_view.z);
}
