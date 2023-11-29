/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_color.hh"
#include "BLI_cpp_type_make.hh"
#include "BLI_cpp_types_make.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"

#include "BKE_cpp_types.hh"
#include "BKE_geometry_set.hh"
#include "BKE_instances.hh"
#include "BKE_node_socket_value_cpp_type.hh"

#include "DNA_meshdata_types.h"

#include "FN_init.h"

struct Tex;
struct Image;
struct Material;

BLI_CPP_TYPE_MAKE(blender::bke::GeometrySet, CPPTypeFlags::Printable);
BLI_CPP_TYPE_MAKE(blender::bke::InstanceReference, CPPTypeFlags::None)

BLI_VECTOR_CPP_TYPE_MAKE(blender::bke::GeometrySet);

BLI_CPP_TYPE_MAKE(Object *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Collection *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Tex *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Image *, CPPTypeFlags::BasicType)
BLI_CPP_TYPE_MAKE(Material *, CPPTypeFlags::BasicType)

BLI_CPP_TYPE_MAKE(MStringProperty, CPPTypeFlags::None);

BLI_CPP_TYPE_MAKE(blender::bke::AnonymousAttributeSet, CPPTypeFlags::None);

void BKE_cpp_types_init()
{
  blender::register_cpp_types();
  FN_register_cpp_types();

  BLI_CPP_TYPE_REGISTER(blender::bke::GeometrySet);
  BLI_CPP_TYPE_REGISTER(blender::bke::InstanceReference);

  BLI_VECTOR_CPP_TYPE_REGISTER(blender::bke::GeometrySet);

  BLI_CPP_TYPE_REGISTER(Object *);
  BLI_CPP_TYPE_REGISTER(Collection *);
  BLI_CPP_TYPE_REGISTER(Tex *);
  BLI_CPP_TYPE_REGISTER(Image *);
  BLI_CPP_TYPE_REGISTER(Material *);

  BLI_CPP_TYPE_REGISTER(MStringProperty);

  BLI_CPP_TYPE_REGISTER(blender::bke::AnonymousAttributeSet);
}

SOCKET_VALUE_CPP_TYPE_MAKE(float);
SOCKET_VALUE_CPP_TYPE_MAKE(blender::float2);
SOCKET_VALUE_CPP_TYPE_MAKE(blender::float3);
SOCKET_VALUE_CPP_TYPE_MAKE(blender::ColorGeometry4f);
SOCKET_VALUE_CPP_TYPE_MAKE(blender::ColorGeometry4b);
SOCKET_VALUE_CPP_TYPE_MAKE(blender::math::Quaternion);
SOCKET_VALUE_CPP_TYPE_MAKE(bool);
SOCKET_VALUE_CPP_TYPE_MAKE(int8_t);
SOCKET_VALUE_CPP_TYPE_MAKE(int32_t);
SOCKET_VALUE_CPP_TYPE_MAKE(blender::int2);
SOCKET_VALUE_CPP_TYPE_MAKE(std::string);

BLI_VECTOR_CPP_TYPE_MAKE(blender::bke::ValueOrField<std::string>);

void FN_register_cpp_types()
{
  SOCKET_VALUE_CPP_TYPE_REGISTER(float);
  SOCKET_VALUE_CPP_TYPE_REGISTER(blender::float2);
  SOCKET_VALUE_CPP_TYPE_REGISTER(blender::float3);
  SOCKET_VALUE_CPP_TYPE_REGISTER(blender::ColorGeometry4f);
  SOCKET_VALUE_CPP_TYPE_REGISTER(blender::ColorGeometry4b);
  SOCKET_VALUE_CPP_TYPE_REGISTER(blender::math::Quaternion);
  SOCKET_VALUE_CPP_TYPE_REGISTER(bool);
  SOCKET_VALUE_CPP_TYPE_REGISTER(int8_t);
  SOCKET_VALUE_CPP_TYPE_REGISTER(int32_t);
  SOCKET_VALUE_CPP_TYPE_REGISTER(blender::int2);
  SOCKET_VALUE_CPP_TYPE_REGISTER(std::string);

  BLI_VECTOR_CPP_TYPE_REGISTER(blender::bke::ValueOrField<std::string>);
}
