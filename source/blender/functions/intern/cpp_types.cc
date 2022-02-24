/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_cpp_type_make.hh"
#include "FN_field_cpp_type.hh"

#include "BLI_color.hh"
#include "BLI_float4x4.hh"
#include "BLI_math_vec_types.hh"

namespace blender::fn {

MAKE_CPP_TYPE(bool, bool, CPPTypeFlags::BasicType)

MAKE_CPP_TYPE(float, float, CPPTypeFlags::BasicType)
MAKE_CPP_TYPE(float2, blender::float2, CPPTypeFlags::BasicType)
MAKE_CPP_TYPE(float3, blender::float3, CPPTypeFlags::BasicType)
MAKE_CPP_TYPE(float4x4, blender::float4x4, CPPTypeFlags::BasicType)

MAKE_CPP_TYPE(int32, int32_t, CPPTypeFlags::BasicType)
MAKE_CPP_TYPE(int8, int8_t, CPPTypeFlags::BasicType)
MAKE_CPP_TYPE(uint32, uint32_t, CPPTypeFlags::BasicType)
MAKE_CPP_TYPE(uint8, uint8_t, CPPTypeFlags::BasicType)

MAKE_CPP_TYPE(ColorGeometry4f, blender::ColorGeometry4f, CPPTypeFlags::BasicType)
MAKE_CPP_TYPE(ColorGeometry4b, blender::ColorGeometry4b, CPPTypeFlags::BasicType)

MAKE_CPP_TYPE(string, std::string, CPPTypeFlags::BasicType)

MAKE_FIELD_CPP_TYPE(FloatField, float);
MAKE_FIELD_CPP_TYPE(Float2Field, float2);
MAKE_FIELD_CPP_TYPE(Float3Field, float3);
MAKE_FIELD_CPP_TYPE(ColorGeometry4fField, blender::ColorGeometry4f);
MAKE_FIELD_CPP_TYPE(BoolField, bool);
MAKE_FIELD_CPP_TYPE(Int8Field, int8_t);
MAKE_FIELD_CPP_TYPE(Int32Field, int32_t);
MAKE_FIELD_CPP_TYPE(StringField, std::string);

}  // namespace blender::fn
