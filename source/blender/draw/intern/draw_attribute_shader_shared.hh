/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#ifndef GPU_SHADER
#  include "GPU_shader_shared_utils.hh"
#endif

#ifdef GPU_SHADER
#  include "gpu_shader_compat.hh"
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

struct [[host_shared]] StoredFloat4 {
  float x;
  float y;
  float z;
  float w;

  float4 load_data() const
  {
    return float4(x, y, z, w);
  }
};

struct [[host_shared]] StoredFloat3 {
  float x;
  float y;
  float z;

  float3 load_data() const
  {
    return float3(x, y, z);
  }
};

struct [[host_shared]] StoredFloat2 {
  float x;
  float y;

  float2 load_data() const
  {
    return float2(x, y);
  }
};

struct [[host_shared]] StoredFloat {
  float x;

  float load_data() const
  {
    return x;
  }
};

struct [[host_shared]] StoredNormalLQ {
  uint xyz;

#ifdef GPU_SHADER /* CPU doesn't have the packing functions. */
  float3 load_data() const
  {
    return float3(unpackSnorm3x10_1x2(xyz).xyz);
  }

  static StoredNormalLQ as_data(float3 interp)
  {
    StoredNormalLQ data;
    data.xyz = packSnorm3x10_1x2(interp.xyzz);
    return data;
  }
#endif
};

struct [[host_shared]] StoredNormalHQ {
  uint xy;
  uint zw;

#ifdef GPU_SHADER /* CPU doesn't have the packing functions. */
  float3 load_data() const
  {
    return float3(unpackSnorm2x16(xy), unpackSnorm2x16(zw).x);
  }

  static StoredNormalHQ as_data(float3 interp)
  {
    StoredNormalHQ data;
    data.xy = packSnorm2x16(interp.xy);
    data.zw = packSnorm2x16(interp.zz);
    return data;
  }
#endif
};

/* NOTE: Legacy, use the member function instead. */
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

/* `uint` versions of the above structs to avoid special behavior when copying types that are not
 * actually floats.  */

struct StoredUint {
  uint x;
};

struct StoredUint2 {
  uint x, y;
};

struct StoredUint3 {
  uint x, y, z;
};

struct StoredUint4 {
  uint x, y, z, w;
};

/** \} */
