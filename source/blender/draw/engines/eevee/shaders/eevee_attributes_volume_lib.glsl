/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_infos.hh"

#include "draw_object_infos_lib.glsl"
#include "eevee_geom_types_lib.bsl.hh"

/* -------------------------------------------------------------------- */
/** \name Volume
 *
 * Volume objects loads attributes from "grids" in the form of 3D textures.
 * Per grid transform order is following loading order.
 * \{ */

float3 grid_coordinates(float3 lP, int index)
{
  const auto &drw_volume = buffer_get(draw_volume_infos, drw_volume);
  return (drw_volume.grids_xform[index] * float4(lP, 1.0f)).xyz;
}

float3 attr_load_orco(VolumePoint point, sampler3D /*tex*/, int /*index*/)
{
  /* NOTE: Doesn't support ORCO attribute. */
  return drw_object_orco(point.lP);
}
float4 attr_load_tangent(VolumePoint /*point*/, sampler3D /*tex*/, int /*index*/)
{
  return float4(0);
}
float4 attr_load_float4(VolumePoint point, sampler3D tex, int index)
{
  return texture(tex, grid_coordinates(point.lP, index));
}
float3 attr_load_float3(VolumePoint point, sampler3D tex, int index)
{
  return texture(tex, grid_coordinates(point.lP, index)).rgb;
}
float2 attr_load_float2(VolumePoint point, sampler3D tex, int index)
{
  return texture(tex, grid_coordinates(point.lP, index)).rg;
}
float attr_load_float(VolumePoint point, sampler3D tex, int index)
{
  return texture(tex, grid_coordinates(point.lP, index)).r;
}
float4 attr_load_color(VolumePoint point, sampler3D tex, int index)
{
  return texture(tex, grid_coordinates(point.lP, index));
}
float3 attr_load_uv(VolumePoint /*point*/, sampler3D /*attr*/, int /*index*/)
{
  return float3(0);
}

/** \} */
