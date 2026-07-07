/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#pragma once

#include "BLI_assert.h"
#include "GPU_material.hh"

namespace blender::gpu {

enum MTLInterfaceDataType {
  MTL_DATATYPE_CHAR,
  MTL_DATATYPE_CHAR2,
  MTL_DATATYPE_CHAR3,
  MTL_DATATYPE_CHAR4,

  MTL_DATATYPE_UCHAR,
  MTL_DATATYPE_UCHAR2,
  MTL_DATATYPE_UCHAR3,
  MTL_DATATYPE_UCHAR4,

  MTL_DATATYPE_BOOL,
  MTL_DATATYPE_BOOL2,
  MTL_DATATYPE_BOOL3,
  MTL_DATATYPE_BOOL4,

  MTL_DATATYPE_SHORT,
  MTL_DATATYPE_SHORT2,
  MTL_DATATYPE_SHORT3,
  MTL_DATATYPE_SHORT4,

  MTL_DATATYPE_USHORT,
  MTL_DATATYPE_USHORT2,
  MTL_DATATYPE_USHORT3,
  MTL_DATATYPE_USHORT4,

  MTL_DATATYPE_INT,
  MTL_DATATYPE_INT2,
  MTL_DATATYPE_INT3,
  MTL_DATATYPE_INT4,

  MTL_DATATYPE_UINT,
  MTL_DATATYPE_UINT2,
  MTL_DATATYPE_UINT3,
  MTL_DATATYPE_UINT4,

  MTL_DATATYPE_FLOAT,
  MTL_DATATYPE_FLOAT2,
  MTL_DATATYPE_FLOAT3,
  MTL_DATATYPE_FLOAT4,
  MTL_DATATYPE_PACKED_FLOAT2,
  MTL_DATATYPE_PACKED_FLOAT3,

  MTL_DATATYPE_LONG,
  MTL_DATATYPE_LONG2,
  MTL_DATATYPE_LONG3,
  MTL_DATATYPE_LONG4,

  MTL_DATATYPE_ULONG,
  MTL_DATATYPE_ULONG2,
  MTL_DATATYPE_ULONG3,
  MTL_DATATYPE_ULONG4,

  MTL_DATATYPE_HALF2x2,
  MTL_DATATYPE_HALF2x3,
  MTL_DATATYPE_HALF2x4,
  MTL_DATATYPE_HALF3x2,
  MTL_DATATYPE_HALF3x3,
  MTL_DATATYPE_HALF3x4,
  MTL_DATATYPE_HALF4x2,
  MTL_DATATYPE_HALF4x3,
  MTL_DATATYPE_HALF4x4,

  MTL_DATATYPE_FLOAT2x2,
  MTL_DATATYPE_FLOAT2x3,
  MTL_DATATYPE_FLOAT2x4,
  MTL_DATATYPE_FLOAT3x2,
  MTL_DATATYPE_FLOAT3x3,
  MTL_DATATYPE_FLOAT3x4,
  MTL_DATATYPE_FLOAT4x2,
  MTL_DATATYPE_FLOAT4x3,
  MTL_DATATYPE_FLOAT4x4,

  MTL_DATATYPE_UINT1010102_NORM,
  MTL_DATATYPE_INT1010102_NORM
};

inline uint mtl_get_data_type_size(MTLInterfaceDataType type)
{
  switch (type) {
    case MTL_DATATYPE_CHAR:
    case MTL_DATATYPE_UCHAR:
    case MTL_DATATYPE_BOOL:
      return 1;
    case MTL_DATATYPE_CHAR2:
    case MTL_DATATYPE_UCHAR2:
    case MTL_DATATYPE_BOOL2:
    case MTL_DATATYPE_SHORT:
    case MTL_DATATYPE_USHORT:
      return 2;

    case MTL_DATATYPE_CHAR3:
    case MTL_DATATYPE_UCHAR3:
    case MTL_DATATYPE_BOOL3:
      return 3;
    case MTL_DATATYPE_CHAR4:
    case MTL_DATATYPE_UCHAR4:
    case MTL_DATATYPE_INT:
    case MTL_DATATYPE_UINT:
    case MTL_DATATYPE_BOOL4:
    case MTL_DATATYPE_SHORT2:
    case MTL_DATATYPE_USHORT2:
    case MTL_DATATYPE_FLOAT:
    case MTL_DATATYPE_UINT1010102_NORM:
    case MTL_DATATYPE_INT1010102_NORM:
      return 4;

    case MTL_DATATYPE_SHORT3:
    case MTL_DATATYPE_USHORT3:
    case MTL_DATATYPE_SHORT4:
    case MTL_DATATYPE_USHORT4:
    case MTL_DATATYPE_INT2:
    case MTL_DATATYPE_UINT2:
    case MTL_DATATYPE_FLOAT2:
    case MTL_DATATYPE_LONG:
    case MTL_DATATYPE_ULONG:
    case MTL_DATATYPE_HALF2x2:
    case MTL_DATATYPE_PACKED_FLOAT2:
      return 8;

    case MTL_DATATYPE_HALF3x2:
    case MTL_DATATYPE_PACKED_FLOAT3:
      return 12;

    case MTL_DATATYPE_INT3:
    case MTL_DATATYPE_INT4:
    case MTL_DATATYPE_UINT3:
    case MTL_DATATYPE_UINT4:
    case MTL_DATATYPE_FLOAT3:
    case MTL_DATATYPE_FLOAT4:
    case MTL_DATATYPE_LONG2:
    case MTL_DATATYPE_ULONG2:
    case MTL_DATATYPE_HALF2x3:
    case MTL_DATATYPE_HALF2x4:
    case MTL_DATATYPE_HALF4x2:
      return 16;

    case MTL_DATATYPE_HALF3x3:
    case MTL_DATATYPE_HALF3x4:
    case MTL_DATATYPE_FLOAT3x2:
      return 24;

    case MTL_DATATYPE_LONG3:
    case MTL_DATATYPE_LONG4:
    case MTL_DATATYPE_ULONG3:
    case MTL_DATATYPE_ULONG4:
    case MTL_DATATYPE_HALF4x3:
    case MTL_DATATYPE_HALF4x4:
    case MTL_DATATYPE_FLOAT2x3:
    case MTL_DATATYPE_FLOAT2x4:
    case MTL_DATATYPE_FLOAT4x2:
      return 32;

    case MTL_DATATYPE_FLOAT3x3:
    case MTL_DATATYPE_FLOAT3x4:
      return 48;

    case MTL_DATATYPE_FLOAT4x3:
    case MTL_DATATYPE_FLOAT4x4:
      return 64;
    default:
      BLI_assert(false);
      return 0;
  };
}

inline MTLInterfaceDataType to_mtl_type(shader::Type type)
{
  switch (type) {
    case shader::Type::float_t:
      return MTL_DATATYPE_FLOAT;
    case shader::Type::float2_t:
      return MTL_DATATYPE_FLOAT2;
    case shader::Type::float3_t:
      return MTL_DATATYPE_FLOAT3;
    case shader::Type::float4_t:
      return MTL_DATATYPE_FLOAT4;
    case shader::Type::float3x3_t:
      return MTL_DATATYPE_FLOAT3x3;
    case shader::Type::float4x4_t:
      return MTL_DATATYPE_FLOAT4x4;
    case shader::Type::uint_t:
      return MTL_DATATYPE_UINT;
    case shader::Type::uint2_t:
      return MTL_DATATYPE_UINT2;
    case shader::Type::uint3_t:
      return MTL_DATATYPE_UINT3;
    case shader::Type::uint4_t:
      return MTL_DATATYPE_UINT4;
    case shader::Type::int_t:
      return MTL_DATATYPE_INT;
    case shader::Type::int2_t:
      return MTL_DATATYPE_INT2;
    case shader::Type::int3_t:
      return MTL_DATATYPE_INT3;
    case shader::Type::int4_t:
      return MTL_DATATYPE_INT4;
    case shader::Type::bool_t:
      return MTL_DATATYPE_BOOL;
    case shader::Type::float3_10_10_10_2_t:
      return MTL_DATATYPE_UINT1010102_NORM;
    case shader::Type::uchar_t:
      return MTL_DATATYPE_UCHAR;
    case shader::Type::uchar2_t:
      return MTL_DATATYPE_UCHAR2;
    case shader::Type::uchar3_t:
      return MTL_DATATYPE_UCHAR3;
    case shader::Type::uchar4_t:
      return MTL_DATATYPE_UCHAR4;
    case shader::Type::char_t:
      return MTL_DATATYPE_CHAR;
    case shader::Type::char2_t:
      return MTL_DATATYPE_CHAR2;
    case shader::Type::char3_t:
      return MTL_DATATYPE_CHAR3;
    case shader::Type::char4_t:
      return MTL_DATATYPE_CHAR4;
    case shader::Type::ushort_t:
      return MTL_DATATYPE_USHORT;
    case shader::Type::ushort2_t:
      return MTL_DATATYPE_USHORT2;
    case shader::Type::ushort3_t:
      return MTL_DATATYPE_USHORT3;
    case shader::Type::ushort4_t:
      return MTL_DATATYPE_USHORT4;
    case shader::Type::short_t:
      return MTL_DATATYPE_SHORT;
    case shader::Type::short2_t:
      return MTL_DATATYPE_SHORT2;
    case shader::Type::short3_t:
      return MTL_DATATYPE_SHORT3;
    case shader::Type::short4_t:
      return MTL_DATATYPE_SHORT4;
    default:
      BLI_assert(false);
      return MTL_DATATYPE_FLOAT;
  };
}

inline uint mtl_get_data_type_alignment(MTLInterfaceDataType type)
{
  switch (type) {
    case MTL_DATATYPE_CHAR:
    case MTL_DATATYPE_UCHAR:
    case MTL_DATATYPE_BOOL:
      return 1;
    case MTL_DATATYPE_CHAR2:
    case MTL_DATATYPE_UCHAR2:
    case MTL_DATATYPE_BOOL2:
    case MTL_DATATYPE_SHORT:
    case MTL_DATATYPE_USHORT:
      return 2;

    case MTL_DATATYPE_CHAR3:
    case MTL_DATATYPE_UCHAR3:
    case MTL_DATATYPE_BOOL3:
    case MTL_DATATYPE_CHAR4:
    case MTL_DATATYPE_UCHAR4:
    case MTL_DATATYPE_INT:
    case MTL_DATATYPE_UINT:
    case MTL_DATATYPE_BOOL4:
    case MTL_DATATYPE_SHORT2:
    case MTL_DATATYPE_USHORT2:
    case MTL_DATATYPE_FLOAT:
    case MTL_DATATYPE_HALF2x2:
    case MTL_DATATYPE_HALF3x2:
    case MTL_DATATYPE_HALF4x2:
    case MTL_DATATYPE_UINT1010102_NORM:
    case MTL_DATATYPE_INT1010102_NORM:
      return 4;

    case MTL_DATATYPE_SHORT3:
    case MTL_DATATYPE_USHORT3:
    case MTL_DATATYPE_SHORT4:
    case MTL_DATATYPE_USHORT4:
    case MTL_DATATYPE_INT2:
    case MTL_DATATYPE_UINT2:
    case MTL_DATATYPE_FLOAT2:
    case MTL_DATATYPE_PACKED_FLOAT2:
    case MTL_DATATYPE_LONG:
    case MTL_DATATYPE_ULONG:
    case MTL_DATATYPE_HALF2x3:
    case MTL_DATATYPE_HALF2x4:
    case MTL_DATATYPE_HALF3x3:
    case MTL_DATATYPE_HALF3x4:
    case MTL_DATATYPE_HALF4x3:
    case MTL_DATATYPE_HALF4x4:
    case MTL_DATATYPE_FLOAT2x2:
    case MTL_DATATYPE_FLOAT3x2:
    case MTL_DATATYPE_FLOAT4x2:
      return 8;

    case MTL_DATATYPE_INT3:
    case MTL_DATATYPE_INT4:
    case MTL_DATATYPE_UINT3:
    case MTL_DATATYPE_UINT4:
    case MTL_DATATYPE_FLOAT3:
    case MTL_DATATYPE_PACKED_FLOAT3:
    case MTL_DATATYPE_FLOAT4:
    case MTL_DATATYPE_LONG2:
    case MTL_DATATYPE_ULONG2:
    case MTL_DATATYPE_FLOAT2x3:
    case MTL_DATATYPE_FLOAT2x4:
    case MTL_DATATYPE_FLOAT3x3:
    case MTL_DATATYPE_FLOAT3x4:
    case MTL_DATATYPE_FLOAT4x3:
    case MTL_DATATYPE_FLOAT4x4:
      return 16;

    case MTL_DATATYPE_LONG3:
    case MTL_DATATYPE_LONG4:
    case MTL_DATATYPE_ULONG3:
    case MTL_DATATYPE_ULONG4:
      return 32;

    default:
      BLI_assert_msg(false, "Unrecognized MTL datatype.");
      return 0;
  };
}

inline MTLInterfaceDataType gpu_type_to_mtl_type(GPUType type)
{
  switch (type) {
    case GPU_FLOAT:
      return MTL_DATATYPE_FLOAT;
    case GPU_VEC2:
      return MTL_DATATYPE_FLOAT2;
    case GPU_VEC3:
      return MTL_DATATYPE_FLOAT3;
    case GPU_VEC4:
      return MTL_DATATYPE_FLOAT4;
    case GPU_MAT3:
      return MTL_DATATYPE_FLOAT3x3;
    case GPU_MAT4:
      return MTL_DATATYPE_FLOAT4x4;
    default:
      BLI_assert(false && "Other types unsupported");
      return MTL_DATATYPE_FLOAT;
  }
  return MTL_DATATYPE_FLOAT;
}

}  // namespace blender::gpu
