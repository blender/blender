/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_geom_types_lib.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Volume
 *
 * Volume objects loads attributes from "grids" in the form of 3D textures.
 * Per grid transform order is following loading order.
 * \{ */

float3 attr_load_orco(VolumePoint point, sampler3D /*tex*/, int /*index*/)
{
  /* NOTE: Doesn't support ORCO attribute. */
  return point.orco_default;
}
float4 attr_load_tangent(VolumePoint /*point*/, sampler3D /*tex*/, int /*index*/)
{
  return float4(0);
}
float4 attr_load_float4(VolumePoint point, sampler3D tex, int index)
{
  return texture(tex, point.grid_co[index]);
}
float3 attr_load_float3(VolumePoint point, sampler3D tex, int index)
{
  return texture(tex, point.grid_co[index]).rgb;
}
float2 attr_load_float2(VolumePoint point, sampler3D tex, int index)
{
  return texture(tex, point.grid_co[index]).rg;
}
float attr_load_float(VolumePoint point, sampler3D tex, int index)
{
  return texture(tex, point.grid_co[index]).r;
}
float4 attr_load_color(VolumePoint point, sampler3D tex, int index)
{
  return texture(tex, point.grid_co[index]);
}
float3 attr_load_uv(VolumePoint /*point*/, sampler3D /*attr*/, int /*index*/)
{
  return float3(0);
}

/** \} */
