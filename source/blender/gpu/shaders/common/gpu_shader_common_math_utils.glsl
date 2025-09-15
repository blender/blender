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
  outnormal = normalize(normal);
}

void vector_copy(float3 normal, out float3 outnormal)
{
  outnormal = normal;
}
