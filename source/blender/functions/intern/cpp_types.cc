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

#include "FN_cpp_types.hh"

#include "BLI_color.hh"
#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_float4x4.hh"

namespace FN {

MAKE_CPP_TYPE(bool, bool)

MAKE_CPP_TYPE(float, float)
MAKE_CPP_TYPE(float3, blender::float3)
MAKE_CPP_TYPE(float4x4, blender::float4x4)

MAKE_CPP_TYPE(int32, int32_t)
MAKE_CPP_TYPE(uint32, uint32_t)
MAKE_CPP_TYPE(uint8, uint8_t)

MAKE_CPP_TYPE(Color4f, blender::Color4f)
MAKE_CPP_TYPE(Color4b, blender::Color4b)

MAKE_CPP_TYPE(string, std::string)

}  // namespace FN
