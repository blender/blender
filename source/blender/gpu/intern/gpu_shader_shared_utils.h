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
 *
 * The Original Code is Copyright (C) 2022 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Glue definition to make shared declaration of struct & functions work in both C / C++ and GLSL.
 * We use the same vector and matrix types as Blender C++. Some math functions are defined to use
 * the float version to match the GLSL syntax.
 * This file can be used for C & C++ code and the syntax used should follow the same rules.
 * Some preprocessing is done by the GPU back-end to make it GLSL compatible.
 *
 * IMPORTANT:
 * - Don't add trailing comma at the end of the enum. Our custom pre-processor will now trim it
 *   for GLSL.
 * - Always use `u` suffix for enum values. GLSL do not support implicit cast.
 * - Define all values. This is in order to simplify custom pre-processor code.
 * - Always use uint32_t as underlying type.
 * - Use float suffix by default for float literals to avoid double promotion in C++.
 * - Pack one float or int after a vec3/ivec3 to fulfill alignment rules.
 *
 * NOTE: Due to alignment restriction and buggy drivers, do not try to use mat3 inside structs.
 * NOTE: (UBO only) Do not use arrays of float. They are padded to arrays of vec4 and are not worth
 * it. This does not apply to SSBO.
 *
 * IMPORTANT: Do not forget to align mat4, vec3 and vec4 to 16 bytes, and vec2 to 8 bytes.
 *
 * NOTE: You can use bool type using bool1 a int boolean type matching the GLSL type.
 */

#ifdef GPU_SHADER
#  define BLI_STATIC_ASSERT_ALIGN(type_, align_)
#  define BLI_STATIC_ASSERT_SIZE(type_, size_)
#  define static
#  define inline
#  define cosf cos
#  define sinf sin
#  define tanf tan
#  define acosf acos
#  define asinf asin
#  define atanf atan
#  define floorf floor
#  define ceilf ceil
#  define sqrtf sqrt

#  define float2 vec2
#  define float3 vec3
#  define float4 vec4
#  define float4x4 mat4
#  define int2 ivec2
#  define int3 ivec3
#  define int4 ivec4
#  define uint2 uvec2
#  define uint3 uvec3
#  define uint4 uvec4
#  define bool1 bool
#  define bool2 bvec2
#  define bool3 bvec3
#  define bool4 bvec4

#else /* C */
#  pragma once

#  include "BLI_assert.h"

#  ifdef __cplusplus
#    include "BLI_float4x4.hh"
#  else
typedef float float2[2];
typedef float float3[3];
typedef float float4[4];
typedef float float4x4[4][4];
#  endif
typedef int int2[2];
typedef int int3[2];
typedef int int4[4];
typedef uint uint2[2];
typedef uint uint3[3];
typedef uint uint4[4];
typedef int int2[2];
typedef int int3[2];
typedef int int4[4];
typedef int bool1;
typedef int bool2[2];
typedef int bool3[2];
typedef int bool4[4];

#endif
