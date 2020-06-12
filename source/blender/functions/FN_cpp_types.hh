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

#ifndef __FN_CPP_TYPES_HH__
#define __FN_CPP_TYPES_HH__

/** \file
 * \ingroup functions
 *
 * This header provides convenient access to CPPType instances for some core types like integer
 * types.
 */

#include "FN_cpp_type.hh"

namespace blender {
namespace FN {

extern const CPPType &CPPType_bool;

extern const CPPType &CPPType_float;
extern const CPPType &CPPType_float3;
extern const CPPType &CPPType_float4x4;

extern const CPPType &CPPType_int32;
extern const CPPType &CPPType_uint32;
extern const CPPType &CPPType_uint8;

extern const CPPType &CPPType_Color4f;
extern const CPPType &CPPType_Color4b;

extern const CPPType &CPPType_string;

}  // namespace FN
}  // namespace blender

#endif /* __FN_CPP_TYPES_HH__ */
