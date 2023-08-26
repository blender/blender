/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

vec2 matcap_uv_compute(vec3 I, vec3 N, bool flipped)
{
  /* Quick creation of an orthonormal basis */
  float a = 1.0 / (1.0 + I.z);
  float b = -I.x * I.y * a;
  vec3 b1 = vec3(1.0 - I.x * I.x * a, b, -I.x);
  vec3 b2 = vec3(b, 1.0 - I.y * I.y * a, -I.y);
  vec2 matcap_uv = vec2(dot(b1, N), dot(b2, N));
  if (flipped) {
    matcap_uv.x = -matcap_uv.x;
  }
  return matcap_uv * 0.496 + 0.5;
}

vec3 get_matcap_lighting(
    sampler2D diffuse_matcap, sampler2D specular_matcap, vec3 base_color, vec3 N, vec3 I)
{
  bool flipped = world_data.matcap_orientation != 0;
  vec2 uv = matcap_uv_compute(I, N, flipped);

  vec3 diffuse = textureLod(diffuse_matcap, uv, 0.0).rgb;
  vec3 specular = textureLod(specular_matcap, uv, 0.0).rgb;

  return diffuse * base_color + specular * float(world_data.use_specular);
}

vec3 get_matcap_lighting(sampler2DArray matcap, vec3 base_color, vec3 N, vec3 I)
{
  bool flipped = world_data.matcap_orientation != 0;
  vec2 uv = matcap_uv_compute(I, N, flipped);

  vec3 diffuse = textureLod(matcap, vec3(uv, 0.0), 0.0).rgb;
  vec3 specular = textureLod(matcap, vec3(uv, 1.0), 0.0).rgb;

  return diffuse * base_color + specular * float(world_data.use_specular);
}
