/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_geom_types_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name World
 *
 * World has no attributes other than orco.
 * \{ */

float3 attr_load_orco(WorldPoint point, float4 orco, int index)
{
  return -g_data.N;
}
float4 attr_load_tangent(WorldPoint point, float4 tangent, int index)
{
  return float4(0);
}
float4 attr_load_float4(WorldPoint point, float4 attr, int index)
{
  return float4(0);
}
float3 attr_load_float3(WorldPoint point, float3 attr, int index)
{
  return float3(0);
}
float2 attr_load_float2(WorldPoint point, float2 attr, int index)
{
  return float2(0);
}
float attr_load_float(WorldPoint point, float attr, int index)
{
  return 0.0f;
}
float4 attr_load_color(WorldPoint point, float4 attr, int index)
{
  return float4(0);
}
float3 attr_load_uv(WorldPoint point, float3 attr, int index)
{
  return float3(0);
}

/** \} */
