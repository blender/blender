/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_color.hh"
#include "BLI_cpp_type_make.hh"
#include "BLI_cpp_types_make.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"

namespace blender {

static auto &get_vector_from_self_map()
{
  static Map<const CPPType *, const VectorCPPType *> map;
  return map;
}

static auto &get_vector_from_value_map()
{
  static Map<const CPPType *, const VectorCPPType *> map;
  return map;
}

void VectorCPPType::register_self()
{
  get_vector_from_self_map().add_new(&this->self, this);
  get_vector_from_value_map().add_new(&this->value, this);
}

const VectorCPPType *VectorCPPType::get_from_self(const CPPType &self)
{
  const VectorCPPType *type = get_vector_from_self_map().lookup_default(&self, nullptr);
  BLI_assert(type == nullptr || type->self == self);
  return type;
}

const VectorCPPType *VectorCPPType::get_from_value(const CPPType &value)
{
  const VectorCPPType *type = get_vector_from_value_map().lookup_default(&value, nullptr);
  BLI_assert(type == nullptr || type->value == value);
  return type;
}

}  // namespace blender

BLI_CPP_TYPE_MAKE(bool, CPPTypeFlags::BasicType)

BLI_CPP_TYPE_MAKE(float, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(blender::float2, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(blender::float3, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(blender::float4x4, CPPTypeFlags::BasicType)

BLI_CPP_TYPE_MAKE(int8_t, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(int16_t, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(int32_t, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(blender::int2, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(int64_t, CPPTypeFlags::BasicType)

BLI_CPP_TYPE_MAKE(uint8_t, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(uint16_t, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(uint32_t, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(uint64_t, CPPTypeFlags::BasicType)

BLI_CPP_TYPE_MAKE(blender::ColorGeometry4f, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(blender::ColorGeometry4b, CPPTypeFlags::BasicType)

BLI_CPP_TYPE_MAKE(blender::math::Quaternion, CPPTypeFlags::BasicType)

BLI_CPP_TYPE_MAKE(std::string, CPPTypeFlags::BasicType)

BLI_VECTOR_CPP_TYPE_MAKE(std::string)

namespace blender {

void register_cpp_types()
{
  BLI_CPP_TYPE_REGISTER(bool);

  BLI_CPP_TYPE_REGISTER(float);
  BLI_CPP_TYPE_REGISTER(blender::float2);
  BLI_CPP_TYPE_REGISTER(blender::float3);
  BLI_CPP_TYPE_REGISTER(blender::float4x4);

  BLI_CPP_TYPE_REGISTER(int8_t);
  BLI_CPP_TYPE_REGISTER(int16_t);
  BLI_CPP_TYPE_REGISTER(int32_t);
  BLI_CPP_TYPE_REGISTER(blender::int2);
  BLI_CPP_TYPE_REGISTER(int64_t);

  BLI_CPP_TYPE_REGISTER(uint8_t);
  BLI_CPP_TYPE_REGISTER(uint16_t);
  BLI_CPP_TYPE_REGISTER(uint32_t);
  BLI_CPP_TYPE_REGISTER(uint64_t);

  BLI_CPP_TYPE_REGISTER(blender::ColorGeometry4f);
  BLI_CPP_TYPE_REGISTER(blender::ColorGeometry4b);

  BLI_CPP_TYPE_REGISTER(math::Quaternion);

  BLI_CPP_TYPE_REGISTER(std::string);

  BLI_VECTOR_CPP_TYPE_REGISTER(std::string);
}

}  // namespace blender
