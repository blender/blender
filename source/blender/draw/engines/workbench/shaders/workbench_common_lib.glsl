/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

#define EPSILON 0.00001f

#define CAVITY_BUFFER_RANGE 4.0f

/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Sphere-map Transform */
float3 workbench_normal_decode(float4 enc)
{
  float2 fenc = enc.xy * 4.0f - 2.0f;
  float f = dot(fenc, fenc);
  float g = sqrt(1.0f - f / 4.0f);
  float3 n;
  n.xy = fenc * g;
  n.z = 1 - f / 2;
  return n;
}

/* From http://aras-p.info/texts/CompactNormalStorage.html
 * Using Method #4: Sphere-map Transform */
float2 workbench_normal_encode(bool front_face, float3 n)
{
  n = normalize(front_face ? n : -n);
  float p = sqrt(n.z * 8.0f + 8.0f);
  n.xy = clamp(n.xy / p + 0.5f, 0.0f, 1.0f);
  return n.xy;
}

/* Encoding into the alpha of a RGBA16F texture. (10bit mantissa) */
#define TARGET_BITCOUNT 8u
#define METALLIC_BITS 3u /* Metallic channel is less important. */
#define ROUGHNESS_BITS (TARGET_BITCOUNT - METALLIC_BITS)

/* Encode 2 float into 1 with the desired precision. */
float workbench_float_pair_encode(float v1, float v2)
{
  // constexpr uint v1_mask = ~(0xFFFFFFFFu << ROUGHNESS_BITS);
  // constexpr uint v2_mask = ~(0xFFFFFFFFu << METALLIC_BITS);
  /* Same as above because some compiler are very dumb and think we use medium int. */
  constexpr int v1_mask = 0x1F;
  constexpr int v2_mask = 0x7;
  int iv1 = int(v1 * float(v1_mask));
  int iv2 = int(v2 * float(v2_mask)) << int(ROUGHNESS_BITS);
  return float(iv1 | iv2);
}

void workbench_float_pair_decode(float data, out float v1, out float v2)
{
  // constexpr uint v1_mask = ~(0xFFFFFFFFu << ROUGHNESS_BITS);
  // constexpr uint v2_mask = ~(0xFFFFFFFFu << METALLIC_BITS);
  /* Same as above because some compiler are very dumb and think we use medium int. */
  constexpr int v1_mask = 0x1F;
  constexpr int v2_mask = 0x7;
  int idata = int(data);
  v1 = float(idata & v1_mask) * (1.0f / float(v1_mask));
  v2 = float(idata >> int(ROUGHNESS_BITS)) * (1.0f / float(v2_mask));
}
