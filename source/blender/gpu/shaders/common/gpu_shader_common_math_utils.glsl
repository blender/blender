/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

void invert_z(float3 v, out float3 outv)
{
  v.z = -v.z;
  outv = v;
}

void vector_normalize(float3 normal, out float3 outnormal)
{
  /* Match the safe normalize function in Cycles by defaulting to float3(0.0f) */
  float length_sqr = dot(normal, normal);
  outnormal = (length_sqr > 1e-35f) ? normal * inversesqrt(length_sqr) : float3(0.0f);
}

void vector_copy(float3 normal, out float3 outnormal)
{
  outnormal = normal;
}
