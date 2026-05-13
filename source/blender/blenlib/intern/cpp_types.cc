/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_color_types.hh"
#include "BLI_cpp_type_make.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_unique_hash.hh"

namespace blender {

template<> void hash_unique_default(const std::string &value, UniqueHashBytes &hash)
{
  hash.data.extend(Span(value.data(), value.size()).cast<std::byte>());
}

void register_cpp_types()
{
  BLI_CPP_TYPE_REGISTER(bool, CPPTypeFlags::BasicType);

  BLI_CPP_TYPE_REGISTER(float, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(float2, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(float3, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(float4, CPPTypeFlags::BasicType);

  BLI_CPP_TYPE_REGISTER(float4x4, CPPTypeFlags::BasicType);

  BLI_CPP_TYPE_REGISTER(int8_t, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(int16_t, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(int32_t, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(short2, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(int2, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(int3, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(int4, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(int64_t, CPPTypeFlags::BasicType);

  BLI_CPP_TYPE_REGISTER(uint8_t, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(uint16_t, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(uint32_t, CPPTypeFlags::BasicType);

  BLI_CPP_TYPE_REGISTER(uint64_t, CPPTypeFlags::BasicType);

  BLI_CPP_TYPE_REGISTER(ColorGeometry4f, CPPTypeFlags::BasicType);
  BLI_CPP_TYPE_REGISTER(ColorGeometry4b, CPPTypeFlags::BasicType);

  BLI_CPP_TYPE_REGISTER(math::Quaternion,
                        CPPTypeFlags::BasicType | CPPTypeFlags::IdentityDefaultValue);

  BLI_CPP_TYPE_REGISTER(std::string, CPPTypeFlags::BasicType);
}

}  // namespace blender
