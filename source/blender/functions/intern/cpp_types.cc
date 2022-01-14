/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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
MAKE_FIELD_CPP_TYPE(Int32Field, int32_t);
MAKE_FIELD_CPP_TYPE(StringField, std::string);

}  // namespace blender::fn
