/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_memory_layout.hh"

namespace blender::gpu {

uint32_t Std430::component_mem_size(const shader::Type /*type*/)
{
  return 4;
}

uint32_t Std430::element_alignment(const shader::Type type, const bool is_array)
{
  if (is_array) {
    return 16;
  }
  switch (type) {
    case shader::Type::FLOAT:
    case shader::Type::UINT:
    case shader::Type::INT:
    case shader::Type::BOOL:
      return 4;
    case shader::Type::VEC2:
    case shader::Type::UVEC2:
    case shader::Type::IVEC2:
      return 8;
    case shader::Type::VEC3:
    case shader::Type::UVEC3:
    case shader::Type::IVEC3:
    case shader::Type::VEC4:
    case shader::Type::UVEC4:
    case shader::Type::IVEC4:
    case shader::Type::MAT3:
    case shader::Type::MAT4:
      return 16;
    default:
      BLI_assert_msg(false, "Type not supported in dynamic structs.");
  }
  return 0;
}

uint32_t Std430::element_components_len(const shader::Type type)
{
  switch (type) {
    case shader::Type::FLOAT:
    case shader::Type::UINT:
    case shader::Type::INT:
    case shader::Type::BOOL:
      return 1;
    case shader::Type::VEC2:
    case shader::Type::UVEC2:
    case shader::Type::IVEC2:
      return 2;
    case shader::Type::VEC3:
    case shader::Type::UVEC3:
    case shader::Type::IVEC3:
    case shader::Type::VEC4:
    case shader::Type::UVEC4:
    case shader::Type::IVEC4:
      return 4;
    case shader::Type::MAT3:
      return 12;
    case shader::Type::MAT4:
      return 16;
    default:
      BLI_assert_msg(false, "Type not supported in dynamic structs.");
  }
  return 0;
}

uint32_t Std430::array_components_len(const shader::Type type)
{
  return Std430::element_components_len(type);
}

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
    case shader::Type::FLOAT:
    case shader::Type::UINT:
    case shader::Type::INT:
    case shader::Type::BOOL:
      return 4;
    case shader::Type::VEC2:
    case shader::Type::UVEC2:
    case shader::Type::IVEC2:
      return 8;
    case shader::Type::VEC3:
    case shader::Type::UVEC3:
    case shader::Type::IVEC3:
    case shader::Type::VEC4:
    case shader::Type::UVEC4:
    case shader::Type::IVEC4:
    case shader::Type::MAT3:
    case shader::Type::MAT4:
      return 16;
    default:
      BLI_assert_msg(false, "Type not supported in dynamic structs.");
  }
  return 0;
}

uint32_t Std140::element_components_len(const shader::Type type)
{
  switch (type) {
    case shader::Type::FLOAT:
    case shader::Type::UINT:
    case shader::Type::INT:
    case shader::Type::BOOL:
      return 1;
    case shader::Type::VEC2:
    case shader::Type::UVEC2:
    case shader::Type::IVEC2:
      return 2;
    case shader::Type::VEC3:
    case shader::Type::UVEC3:
    case shader::Type::IVEC3:
      return 3;
    case shader::Type::VEC4:
    case shader::Type::UVEC4:
    case shader::Type::IVEC4:
      return 4;
    case shader::Type::MAT3:
      return 12;
    case shader::Type::MAT4:
      return 16;
    default:
      BLI_assert_msg(false, "Type not supported in dynamic structs.");
  }
  return 0;
}

uint32_t Std140::array_components_len(const shader::Type type)
{
  switch (type) {
    case shader::Type::FLOAT:
    case shader::Type::UINT:
    case shader::Type::INT:
    case shader::Type::BOOL:
    case shader::Type::VEC2:
    case shader::Type::UVEC2:
    case shader::Type::IVEC2:
    case shader::Type::VEC3:
    case shader::Type::UVEC3:
    case shader::Type::IVEC3:
    case shader::Type::VEC4:
    case shader::Type::UVEC4:
    case shader::Type::IVEC4:
      return 4;
    case shader::Type::MAT3:
      return 12;
    case shader::Type::MAT4:
      return 16;
    default:
      BLI_assert_msg(false, "Type not supported in dynamic structs.");
  }
  return 0;
}

}  // namespace blender::gpu
