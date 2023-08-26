/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/sample/mapping.h"

CCL_NAMESPACE_BEGIN

/* Light Sample Result */

typedef struct LightSample {
  float3 P;            /* position on light, or direction for distant light */
  float3 Ng;           /* normal on light */
  float3 D;            /* direction from shading point to light */
  float t;             /* distance to light (FLT_MAX for distant light) */
  float u, v;          /* parametric coordinate on primitive */
  float pdf;           /* pdf for selecting light and point on light */
  float pdf_selection; /* pdf for selecting light */
  float eval_fac;      /* intensity multiplier */
  int object;          /* object id for triangle/curve lights */
  int prim;            /* primitive id for triangle/curve lights */
  int shader;          /* shader id */
  int lamp;            /* lamp id */
  int group;           /* lightgroup */
  LightType type;      /* type of light */
} LightSample;

/* Utilities */

ccl_device_inline float3 ellipse_sample(float3 ru, float3 rv, float2 rand)
{
  const float2 uv = sample_uniform_disk(rand);
  return ru * uv.x + rv * uv.y;
}

ccl_device_inline float3 rectangle_sample(float3 ru, float3 rv, float2 rand)
{
  return ru * (2.0f * rand.x - 1.0f) + rv * (2.0f * rand.y - 1.0f);
}

ccl_device float3 disk_light_sample(float3 n, float2 rand)
{
  float3 ru, rv;

  make_orthonormals(n, &ru, &rv);

  return ellipse_sample(ru, rv, rand);
}

ccl_device float lamp_light_pdf(const float3 Ng, const float3 I, float t)
{
  float cos_pi = dot(Ng, I);

  if (cos_pi <= 0.0f)
    return 0.0f;

  return t * t / cos_pi;
}

/* Visibility flag om the light shader. */
ccl_device_inline bool is_light_shader_visible_to_path(const int shader, const uint32_t path_flag)
{
  if ((shader & SHADER_EXCLUDE_ANY) == 0) {
    return true;
  }

  if (((shader & SHADER_EXCLUDE_DIFFUSE) && (path_flag & PATH_RAY_DIFFUSE)) ||
      ((shader & SHADER_EXCLUDE_GLOSSY) && ((path_flag & (PATH_RAY_GLOSSY | PATH_RAY_REFLECT)) ==
                                            (PATH_RAY_GLOSSY | PATH_RAY_REFLECT))) ||
      ((shader & SHADER_EXCLUDE_TRANSMIT) && (path_flag & PATH_RAY_TRANSMIT)) ||
      ((shader & SHADER_EXCLUDE_CAMERA) && (path_flag & PATH_RAY_CAMERA)) ||
      ((shader & SHADER_EXCLUDE_SCATTER) && (path_flag & PATH_RAY_VOLUME_SCATTER)))
  {
    return false;
  }

  return true;
}

CCL_NAMESPACE_END
