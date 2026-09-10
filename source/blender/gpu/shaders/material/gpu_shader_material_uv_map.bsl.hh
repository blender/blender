/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

[[node]]
void node_uvmap(float4 attr_uv, float3 &outvec)
{
  outvec = attr_uv.xyz;
}
