/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_color.hh"
#include "BLI_cpp_type_make.hh"
#include "BLI_cpp_types_make.hh"
#include "BLI_float4x4.hh"
#include "BLI_math_vector_types.hh"

#include "FN_field_cpp_type_make.hh"
#include "FN_init.h"

FN_FIELD_CPP_TYPE_MAKE(float);
FN_FIELD_CPP_TYPE_MAKE(blender::float2);
FN_FIELD_CPP_TYPE_MAKE(blender::float3);
FN_FIELD_CPP_TYPE_MAKE(blender::ColorGeometry4f);
FN_FIELD_CPP_TYPE_MAKE(blender::ColorGeometry4b);
FN_FIELD_CPP_TYPE_MAKE(bool);
FN_FIELD_CPP_TYPE_MAKE(int8_t);
FN_FIELD_CPP_TYPE_MAKE(int32_t);
FN_FIELD_CPP_TYPE_MAKE(std::string);

BLI_VECTOR_CPP_TYPE_MAKE(blender::fn::ValueOrField<std::string>);

void FN_register_cpp_types()
{
  FN_FIELD_CPP_TYPE_REGISTER(float);
  FN_FIELD_CPP_TYPE_REGISTER(blender::float2);
  FN_FIELD_CPP_TYPE_REGISTER(blender::float3);
  FN_FIELD_CPP_TYPE_REGISTER(blender::ColorGeometry4f);
  FN_FIELD_CPP_TYPE_REGISTER(blender::ColorGeometry4b);
  FN_FIELD_CPP_TYPE_REGISTER(bool);
  FN_FIELD_CPP_TYPE_REGISTER(int8_t);
  FN_FIELD_CPP_TYPE_REGISTER(int32_t);
  FN_FIELD_CPP_TYPE_REGISTER(std::string);

  BLI_VECTOR_CPP_TYPE_REGISTER(blender::fn::ValueOrField<std::string>);
}
