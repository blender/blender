/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/* ---------------------------------------------------------------------- */
/** \name Comparison
 * \{ */

float reduce_max(float2 a)
{
  return max(a.x, a.y);
}
float reduce_max(float3 a)
{
  return max(a.x, max(a.y, a.z));
}
float reduce_max(float4 a)
{
  return max(max(a.x, a.y), max(a.z, a.w));
}
int reduce_max(int2 a)
{
  return max(a.x, a.y);
}
int reduce_max(int3 a)
{
  return max(a.x, max(a.y, a.z));
}
int reduce_max(int4 a)
{
  return max(max(a.x, a.y), max(a.z, a.w));
}

float reduce_min(float2 a)
{
  return min(a.x, a.y);
}
float reduce_min(float3 a)
{
  return min(a.x, min(a.y, a.z));
}
float reduce_min(float4 a)
{
  return min(min(a.x, a.y), min(a.z, a.w));
}
int reduce_min(int2 a)
{
  return min(a.x, a.y);
}
int reduce_min(int3 a)
{
  return min(a.x, min(a.y, a.z));
}
int reduce_min(int4 a)
{
  return min(min(a.x, a.y), min(a.z, a.w));
}

float reduce_add(float2 a)
{
  return a.x + a.y;
}
float reduce_add(float3 a)
{
  return a.x + a.y + a.z;
}
float reduce_add(float4 a)
{
  return a.x + a.y + a.z + a.w;
}
int reduce_add(int2 a)
{
  return a.x + a.y;
}
int reduce_add(int3 a)
{
  return a.x + a.y + a.z;
}
int reduce_add(int4 a)
{
  return a.x + a.y + a.z + a.w;
}

#if 0 /* Remove unused variants as they are slow down compilation. */
float reduce_mul(float2 a)
{
  return a.x * a.y;
}
float reduce_mul(float3 a)
{
  return a.x * a.y * a.z;
}
float reduce_mul(float4 a)
{
  return a.x * a.y * a.z * a.w;
}
int reduce_mul(int2 a)
{
  return a.x * a.y;
}
int reduce_mul(int3 a)
{
  return a.x * a.y * a.z;
}
int reduce_mul(int4 a)
{
  return a.x * a.y * a.z * a.w;
}
#endif

float average(float2 a)
{
  return reduce_add(a) * (1.0f / 2.0f);
}
float average(float3 a)
{
  return reduce_add(a) * (1.0f / 3.0f);
}
float average(float4 a)
{
  return reduce_add(a) * (1.0f / 4.0f);
}

/** \} */
