/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifndef GPU_SHADER
#  include "GPU_shader_shared_utils.hh"
#endif

/* Copy of DNA enum in `DNA_curves_types.h`. */
enum [[host_shared]] CurveType : uint32_t {
  CURVE_TYPE_CATMULL_ROM = 0u,
  CURVE_TYPE_POLY = 1u,
  CURVE_TYPE_BEZIER = 2u,
  CURVE_TYPE_NURBS = 3u,
};

/* -------------------------------------------------------------------- */
/** \name Generic Attribute
 *
 * These types are necessary to overcome the issue with float3 alignment on GPU.
 * Having all types using the same interface allows the usage of templates to load and manipulate
 * them inside the shaders.
 * \{ */

struct StoredFloat4 {
  float x;
  float y;
  float z;
  float w;
};

struct StoredFloat3 {
  float x;
  float y;
  float z;
};

struct StoredFloat2 {
  float x;
  float y;
};

struct StoredFloat {
  float x;
};

float4 load_data(StoredFloat4 data)
{
  return float4(data.x, data.y, data.z, data.w);
}

float3 load_data(StoredFloat3 data)
{
  return float3(data.x, data.y, data.z);
}

float2 load_data(StoredFloat2 data)
{
  return float2(data.x, data.y);
}

float load_data(StoredFloat data)
{
  return float(data.x);
}

StoredFloat4 as_data(float4 interp)
{
  StoredFloat4 data;
  data.x = interp.x;
  data.y = interp.y;
  data.z = interp.z;
  data.w = interp.w;
  return data;
}

StoredFloat3 as_data(float3 interp)
{
  StoredFloat3 data;
  data.x = interp.x;
  data.y = interp.y;
  data.z = interp.z;
  return data;
}

StoredFloat2 as_data(float2 interp)
{
  StoredFloat2 data;
  data.x = interp.x;
  data.y = interp.y;
  return data;
}

StoredFloat as_data(float interp)
{
  StoredFloat data;
  data.x = interp;
  return data;
}

/** \} */
