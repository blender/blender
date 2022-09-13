/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_color.hh"
#include "BLI_cpp_type_make.hh"
#include "BLI_float4x4.hh"
#include "BLI_math_vec_types.hh"

#include "FN_field_cpp_type.hh"

MAKE_FIELD_CPP_TYPE(FloatField, float);
MAKE_FIELD_CPP_TYPE(Float2Field, blender::float2);
MAKE_FIELD_CPP_TYPE(Float3Field, blender::float3);
MAKE_FIELD_CPP_TYPE(ColorGeometry4fField, blender::ColorGeometry4f);
MAKE_FIELD_CPP_TYPE(ColorGeometry4bField, blender::ColorGeometry4b);
MAKE_FIELD_CPP_TYPE(BoolField, bool);
MAKE_FIELD_CPP_TYPE(Int8Field, int8_t);
MAKE_FIELD_CPP_TYPE(Int32Field, int32_t);
MAKE_FIELD_CPP_TYPE(StringField, std::string);
BLI_CPP_TYPE_MAKE(StringValueOrFieldVector,
                  blender::Vector<blender::fn::ValueOrField<std::string>>,
                  CPPTypeFlags::None);
