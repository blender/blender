/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

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

ccl_device_inline float3 ellipse_sample(float3 ru, float3 rv, float randu, float randv)
{
  const float2 rand = concentric_sample_disk(randu, randv);
  return ru * rand.x + rv * rand.y;
}

ccl_device_inline float3 rectangle_sample(float3 ru, float3 rv, float randu, float randv)
{
  return ru * (2.0f * randu - 1.0f) + rv * (2.0f * randv - 1.0f);
}

ccl_device float3 disk_light_sample(float3 v, float randu, float randv)
{
  float3 ru, rv;

  make_orthonormals(v, &ru, &rv);

  return ellipse_sample(ru, rv, randu, randv);
}

ccl_device float lamp_light_pdf(const float3 Ng, const float3 I, float t)
{
  float cos_pi = dot(Ng, I);

  if (cos_pi <= 0.0f)
    return 0.0f;

  return t * t / cos_pi;
}

CCL_NAMESPACE_END
