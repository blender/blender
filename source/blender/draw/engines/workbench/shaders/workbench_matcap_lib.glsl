/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/workbench_prepass_info.hh"

#ifdef GPU_LIBRARY_SHADER
SHADER_LIBRARY_CREATE_INFO(workbench_prepass)
#endif

vec2 matcap_uv_compute(vec3 I, vec3 N, bool flipped)
{
  /* Quick creation of an orthonormal basis */
  float a = 1.0f / (1.0f + I.z);
  float b = -I.x * I.y * a;
  vec3 b1 = vec3(1.0f - I.x * I.x * a, b, -I.x);
  vec3 b2 = vec3(b, 1.0f - I.y * I.y * a, -I.y);
  vec2 matcap_uv = vec2(dot(b1, N), dot(b2, N));
  if (flipped) {
    matcap_uv.x = -matcap_uv.x;
  }
  return matcap_uv * 0.496f + 0.5f;
}

vec3 get_matcap_lighting(
    sampler2D diffuse_matcap, sampler2D specular_matcap, vec3 base_color, vec3 N, vec3 I)
{
  bool flipped = world_data.matcap_orientation != 0;
  vec2 uv = matcap_uv_compute(I, N, flipped);

  vec3 diffuse = textureLod(diffuse_matcap, uv, 0.0f).rgb;
  vec3 specular = textureLod(specular_matcap, uv, 0.0f).rgb;

  return diffuse * base_color + specular * float(world_data.use_specular);
}

vec3 get_matcap_lighting(sampler2DArray matcap, vec3 base_color, vec3 N, vec3 I)
{
  bool flipped = world_data.matcap_orientation != 0;
  vec2 uv = matcap_uv_compute(I, N, flipped);

  vec3 diffuse = textureLod(matcap, vec3(uv, 0.0f), 0.0f).rgb;
  vec3 specular = textureLod(matcap, vec3(uv, 1.0f), 0.0f).rgb;

  return diffuse * base_color + specular * float(world_data.use_specular);
}
