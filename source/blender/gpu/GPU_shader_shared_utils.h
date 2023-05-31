/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
 * - Always use `u` suffix for enum values. GLSL do not support implicit cast.
 * - Define all values. This is in order to simplify custom pre-processor code.
 * - (C++ only) Always use `uint32_t` as underlying type (`enum eMyEnum : uint32_t`).
 * - (C only) do NOT use the enum type inside UBO/SSBO structs and use `uint` instead.
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
#  define expf exp

#  define bool1 bool
/* Type name collision with Metal shading language - These type-names are already defined. */
#  ifndef GPU_METAL
#    define float2 vec2
#    define float3 vec3
#    define float4 vec4
#    define float4x4 mat4
#    define int2 ivec2
#    define int3 ivec3
#    define int4 ivec4
#    define uint2 uvec2
#    define uint3 uvec3
#    define uint4 uvec4
#    define bool2 bvec2
#    define bool3 bvec3
#    define bool4 bvec4
#    define packed_float3 vec3
#    define packed_int3 int3
#  endif

#else /* C / C++ */
#  pragma once

#  include "BLI_assert.h"

#  ifdef __cplusplus
#    include "BLI_math_matrix_types.hh"
#    include "BLI_math_vector_types.hh"
using blender::float2;
using blender::float3;
using blender::float4;
using blender::float4x4;
using blender::int2;
using blender::int3;
using blender::int4;
using blender::uint2;
using blender::uint3;
using blender::uint4;
using bool1 = int;
using bool2 = blender::int2;
using bool3 = blender::int3;
using bool4 = blender::int4;
using packed_float3 = blender::float3;
using packed_int3 = blender::int3;

#  else /* C */
typedef float float2[2];
typedef float float3[3];
typedef float float4[4];
typedef float float4x4[4][4];
typedef int int2[2];
typedef int int3[2];
typedef int int4[4];
typedef uint uint2[2];
typedef uint uint3[3];
typedef uint uint4[4];
typedef int bool1;
typedef int bool2[2];
typedef int bool3[2];
typedef int bool4[4];
typedef float3 packed_float3;
typedef int3 packed_int3;
#  endif

#endif
