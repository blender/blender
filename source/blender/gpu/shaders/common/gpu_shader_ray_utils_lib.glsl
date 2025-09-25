/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

/**
 * Ray offset to avoid self intersection.
 *
 * This can be used to compute a modified ray start position for rays leaving from a surface.
 * From:
 * "A Fast and Robust Method for Avoiding Self-Intersection"
 * Ray Tracing Gems, chapter 6.
 */
float3 offset_ray(float3 P, float3 Ng)
{
  constexpr float origin = 1.0f / 32.0f;
  constexpr float float_scale = 1.0f / 65536.0f;
  constexpr float int_scale = 256.0f;

  int3 of_i = int3(int_scale * Ng);
  of_i = int3((P.x < 0.0f) ? -of_i.x : of_i.x,
              (P.y < 0.0f) ? -of_i.y : of_i.y,
              (P.z < 0.0f) ? -of_i.z : of_i.z);
  float3 P_i = intBitsToFloat(floatBitsToInt(P) + of_i);

  float3 uf = P + float_scale * Ng;
  return float3((abs(P.x) < origin) ? uf.x : P_i.x,
                (abs(P.y) < origin) ? uf.y : P_i.y,
                (abs(P.z) < origin) ? uf.z : P_i.z);
}
