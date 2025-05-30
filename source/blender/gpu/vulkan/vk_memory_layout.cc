/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_memory_layout.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Std430 memory layout
 * \{ */

uint32_t Std430::component_mem_size(const shader::Type /*type*/)
{
  return 4;
}

uint32_t Std430::element_alignment(const shader::Type type, const bool /*is_array*/)
{
  switch (type) {
    case shader::Type::float_t:
    case shader::Type::uint_t:
    case shader::Type::int_t:
    case shader::Type::bool_t:
      return 4;
    case shader::Type::float2_t:
    case shader::Type::uint2_t:
    case shader::Type::int2_t:
      return 8;
    case shader::Type::float3_t:
    case shader::Type::uint3_t:
    case shader::Type::int3_t:
    case shader::Type::float4_t:
    case shader::Type::uint4_t:
    case shader::Type::int4_t:
    case shader::Type::float3x3_t:
    case shader::Type::float4x4_t:
      return 16;
    default:
      BLI_assert_msg(false, "Type not supported in dynamic structs.");
  }
  return 0;
}

uint32_t Std430::element_components_len(const shader::Type type)
{
  switch (type) {
    case shader::Type::float_t:
    case shader::Type::uint_t:
    case shader::Type::int_t:
    case shader::Type::bool_t:
      return 1;
    case shader::Type::float2_t:
    case shader::Type::uint2_t:
    case shader::Type::int2_t:
      return 2;
    case shader::Type::float3_t:
    case shader::Type::uint3_t:
    case shader::Type::int3_t:
      return 3;
    case shader::Type::float4_t:
    case shader::Type::uint4_t:
    case shader::Type::int4_t:
      return 4;
    case shader::Type::float3x3_t:
      return 12;
    case shader::Type::float4x4_t:
      return 16;
    default:
      BLI_assert_msg(false, "Type not supported in dynamic structs.");
  }
  return 0;
}

uint32_t Std430::array_components_len(const shader::Type type)
{
  switch (type) {
    case shader::Type::float_t:
    case shader::Type::uint_t:
    case shader::Type::int_t:
    case shader::Type::bool_t:
      return 1;
    case shader::Type::float2_t:
    case shader::Type::uint2_t:
    case shader::Type::int2_t:
      return 2;
    case shader::Type::float3_t:
    case shader::Type::uint3_t:
    case shader::Type::int3_t:
    case shader::Type::float4_t:
    case shader::Type::uint4_t:
    case shader::Type::int4_t:
      return 4;
    case shader::Type::float3x3_t:
      return 12;
    case shader::Type::float4x4_t:
      return 16;
    default:
      BLI_assert_msg(false, "Type not supported in dynamic structs.");
  }
  return 0;
}

uint32_t Std430::inner_row_padding(const shader::Type type)
{
  return type == shader::Type::float3x3_t ? 3 : 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Std140 memory layout
 * \{ */

uint32_t Std140::component_mem_size(const shader::Type /*type*/)
{
  return 4;
}

uint32_t Std140::element_alignment(const shader::Type type, const bool is_array)
{
  if (is_array) {
    return 16;
  }
  switch (type) {
    case shader::Type::float_t:
    case shader::Type::uint_t:
    case shader::Type::int_t:
    case shader::Type::bool_t:
      return 4;
    case shader::Type::float2_t:
    case shader::Type::uint2_t:
    case shader::Type::int2_t:
      return 8;
    case shader::Type::float3_t:
    case shader::Type::uint3_t:
    case shader::Type::int3_t:
    case shader::Type::float4_t:
    case shader::Type::uint4_t:
    case shader::Type::int4_t:
    case shader::Type::float3x3_t:
    case shader::Type::float4x4_t:
      return 16;
    default:
      BLI_assert_msg(false, "Type not supported in dynamic structs.");
  }
  return 0;
}

uint32_t Std140::element_components_len(const shader::Type type)
{
  switch (type) {
    case shader::Type::float_t:
    case shader::Type::uint_t:
    case shader::Type::int_t:
    case shader::Type::bool_t:
      return 1;
    case shader::Type::float2_t:
    case shader::Type::uint2_t:
    case shader::Type::int2_t:
      return 2;
    case shader::Type::float3_t:
    case shader::Type::uint3_t:
    case shader::Type::int3_t:
      return 3;
    case shader::Type::float4_t:
    case shader::Type::uint4_t:
    case shader::Type::int4_t:
      return 4;
    case shader::Type::float3x3_t:
      return 12;
    case shader::Type::float4x4_t:
      return 16;
    default:
      BLI_assert_msg(false, "Type not supported in dynamic structs.");
  }
  return 0;
}

uint32_t Std140::array_components_len(const shader::Type type)
{
  switch (type) {
    case shader::Type::float_t:
    case shader::Type::uint_t:
    case shader::Type::int_t:
    case shader::Type::bool_t:
    case shader::Type::float2_t:
    case shader::Type::uint2_t:
    case shader::Type::int2_t:
    case shader::Type::float3_t:
    case shader::Type::uint3_t:
    case shader::Type::int3_t:
    case shader::Type::float4_t:
    case shader::Type::uint4_t:
    case shader::Type::int4_t:
      return 4;
    case shader::Type::float3x3_t:
      return 12;
    case shader::Type::float4x4_t:
      return 16;
    default:
      BLI_assert_msg(false, "Type not supported in dynamic structs.");
  }
  return 0;
}

uint32_t Std140::inner_row_padding(const shader::Type /*type*/)
{
  return 0;
}

/** \} */

}  // namespace blender::gpu
