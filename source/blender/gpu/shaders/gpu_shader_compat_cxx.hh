/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shading language to C++ stubs.
 *
 * The goal of this header is to make the Shading Language source file compile using a modern C++
 * compiler. This allows for linting and IDE functionalities to work.
 *
 * This file can be included inside any Shading Language file to make the Shading Language syntax
 * to work. Then your IDE must to be configured to associate `.glsl` files to C++ so that the C++
 * linter does the analysis.
 *
 * This is why the implementation of each function is not needed. However, we make sure that type
 * casting is always explicit. This is because implicit casts are not always supported on all
 * implementations.
 *
 * Some of the features of Shading Language are omitted by design. They are either:
 * - Not needed (e.g. per component matrix multiplication).
 * - Against our code-style (e.g. `stpq` swizzle).
 * - Unsupported by our Metal Shading Language layer (e.g. mixed vector-scalar matrix constructor).
 *
 * IMPORTANT: Please ask the module team if you need some feature that are not listed in this file.
 */

#pragma once

#include "gpu_shader_cxx_builtin.hh"  // IWYU pragma: export
#include "gpu_shader_cxx_global.hh"   // IWYU pragma: export
#include "gpu_shader_cxx_image.hh"    // IWYU pragma: export
#include "gpu_shader_cxx_matrix.hh"   // IWYU pragma: export
#include "gpu_shader_cxx_sampler.hh"  // IWYU pragma: export
#include "gpu_shader_cxx_vector.hh"   // IWYU pragma: export

#define assert(assertion)
#define printf(...)

/* -------------------------------------------------------------------- */
/** \name Keywords
 * \{ */

/* Note: Cannot easily mutate them. Pass every by copy for now. */

/* Pass argument by reference. */
#define inout
/* Pass argument by reference but only write to it. Its initial value is undefined. */
#define out
/* Pass argument by copy (default). */
#define in

/* Decorate a variable in global scope that is common to all threads in a thread-group. */
#define shared

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compatibility
 * \{ */

/* Array syntax compatibility. */
/* clang-format off */
#define float_array(...) { __VA_ARGS__ }
#define float2_array(...) { __VA_ARGS__ }
#define float3_array(...) { __VA_ARGS__ }
#define float4_array(...) { __VA_ARGS__ }
#define int_array(...) { __VA_ARGS__ }
#define int2_array(...) { __VA_ARGS__ }
#define int3_array(...) { __VA_ARGS__ }
#define int4_array(...) { __VA_ARGS__ }
#define uint_array(...) { __VA_ARGS__ }
#define uint2_array(...) { __VA_ARGS__ }
#define uint3_array(...) { __VA_ARGS__ }
#define uint4_array(...) { __VA_ARGS__ }
#define bool_array(...) { __VA_ARGS__ }
#define bool2_array(...) { __VA_ARGS__ }
#define bool3_array(...) { __VA_ARGS__ }
#define bool4_array(...) { __VA_ARGS__ }
/* clang-format on */

#define METAL_CONSTRUCTOR_1(class_name, t1, m1) \
  class_name() = default; \
  class_name(t1 m1##_) : m1(m1##_){};

#define METAL_CONSTRUCTOR_2(class_name, t1, m1, t2, m2) \
  class_name() = default; \
  class_name(t1 m1##_, t2 m2##_) : m1(m1##_), m2(m2##_){};

#define METAL_CONSTRUCTOR_3(class_name, t1, m1, t2, m2, t3, m3) \
  class_name() = default; \
  class_name(t1 m1##_, t2 m2##_, t3 m3##_) : m1(m1##_), m2(m2##_), m3(m3##_){};

#define METAL_CONSTRUCTOR_4(class_name, t1, m1, t2, m2, t3, m3, t4, m4) \
  class_name() = default; \
  class_name(t1 m1##_, t2 m2##_, t3 m3##_, t4 m4##_) \
      : m1(m1##_), m2(m2##_), m3(m3##_), m4(m4##_){};

#define METAL_CONSTRUCTOR_5(class_name, t1, m1, t2, m2, t3, m3, t4, m4, t5, m5) \
  class_name() = default; \
  class_name(t1 m1##_, t2 m2##_, t3 m3##_, t4 m4##_, t5 m5##_) \
      : m1(m1##_), m2(m2##_), m3(m3##_), m4(m4##_), m5(m5##_){};

#define METAL_CONSTRUCTOR_6(class_name, t1, m1, t2, m2, t3, m3, t4, m4, t5, m5, t6, m6) \
  class_name() = default; \
  class_name(t1 m1##_, t2 m2##_, t3 m3##_, t4 m4##_, t5 m5##_, t6 m6##_) \
      : m1(m1##_), m2(m2##_), m3(m3##_), m4(m4##_), m5(m5##_), m6(m6##_){};

#define METAL_CONSTRUCTOR_7(class_name, t1, m1, t2, m2, t3, m3, t4, m4, t5, m5, t6, m6, t7, m7) \
  class_name() = default; \
  class_name(t1 m1##_, t2 m2##_, t3 m3##_, t4 m4##_, t5 m5##_, t6 m6##_, t7 m7##_) \
      : m1(m1##_), m2(m2##_), m3(m3##_), m4(m4##_), m5(m5##_), m6(m6##_), m7(m7##_){};

/** \} */

/* Use to suppress `-Wimplicit-fallthrough` (in place of `break`). */
#ifndef ATTR_FALLTHROUGH
#  ifdef __GNUC__
#    define ATTR_FALLTHROUGH __attribute__((fallthrough))
#  else
#    define ATTR_FALLTHROUGH ((void)0)
#  endif
#endif

/* GLSL main function must return void. C++ need to return int.
 * Inject real main (C++) inside the GLSL main definition. */
#define main() \
  /* Fake main prototype. */ \
  /* void */ _fake_main(); \
  /* Real main. */ \
  int main() \
  { \
    _fake_main(); \
    return 0; \
  } \
  /* Fake main definition. */ \
  void _fake_main()

#define GLSL_CPP_STUBS
#ifndef GPU_SHADER
#  define GPU_SHADER
#endif

/* List of reserved keywords in GLSL. */
#define common common_is_reserved_glsl_keyword_do_not_use
#define partition partition_is_reserved_glsl_keyword_do_not_use
#define active active_is_reserved_glsl_keyword_do_not_use
// #define class /* Supported. */
#define union union_is_reserved_glsl_keyword_do_not_use
// #define enum /* Supported. */
#define typedef typedef_is_reserved_glsl_keyword_do_not_use
// #define template /* Needed for Stubs. */
// #define this /* Needed for Stubs. */
#define packed packed_is_reserved_glsl_keyword_do_not_use
#define resource resource_is_reserved_glsl_keyword_do_not_use
#define goto goto_is_reserved_glsl_keyword_do_not_use
// #define inline  /* Supported. */
#define noinline noinline_is_reserved_glsl_keyword_do_not_use
// #define public /* Supported. */
// #define private /* Supported. */
// #define static /* Supported. */
// #define extern /* Needed for Stubs. */
#define external external_is_reserved_glsl_keyword_do_not_use
#define interface interface_is_reserved_glsl_keyword_do_not_use
#define long long_is_reserved_glsl_keyword_do_not_use
// #define short /* Supported. */
// #define half /* Supported. */
#define fixed fixed_is_reserved_glsl_keyword_do_not_use
#define unsigned unsigned_is_reserved_glsl_keyword_do_not_use
#define superp superp_is_reserved_glsl_keyword_do_not_use
#define input input_is_reserved_glsl_keyword_do_not_use
#define output output_is_reserved_glsl_keyword_do_not_use
#define hvec2 hvec2_is_reserved_glsl_keyword_do_not_use
#define hvec3 hvec3_is_reserved_glsl_keyword_do_not_use
#define hvec4 hvec4_is_reserved_glsl_keyword_do_not_use
#define fvec2 fvec2_is_reserved_glsl_keyword_do_not_use
#define fvec3 fvec3_is_reserved_glsl_keyword_do_not_use
#define fvec4 fvec4_is_reserved_glsl_keyword_do_not_use
#define sampler3DRect sampler3DRect_is_reserved_glsl_keyword_do_not_use
#define filter filter_is_reserved_glsl_keyword_do_not_use
#define sizeof sizeof_is_reserved_glsl_keyword_do_not_use
#define cast cast_is_reserved_glsl_keyword_do_not_use
// #define namespace /* Needed for Stubs. */
// #define using /* Needed for Stubs. */
#define row_major row_major_is_reserved_glsl_keyword_do_not_use

#ifdef GPU_SHADER_LIBRARY
#  define GPU_VERTEX_SHADER
#  define GPU_FRAGMENT_SHADER
#  define GPU_COMPUTE_SHADER
#endif

/* Resource accessor. */
#define specialization_constant_get(create_info, _res) create_info::_res
#define shared_variable_get(create_info, _res) create_info::_res
#define push_constant_get(create_info, _res) create_info::_res
#define interface_get(create_info, _res) create_info::_res
#define attribute_get(create_info, _res) create_info::_res
#define buffer_get(create_info, _res) create_info::_res
#define sampler_get(create_info, _res) create_info::_res
#define image_get(create_info, _res) create_info::_res

#include "GPU_shader_shared_utils.hh"

#ifdef __GNUC__
/* Avoid warnings caused by our own unroll attributes. */
#  ifdef __clang__
#    pragma GCC diagnostic ignored "-Wunknown-attributes"
#  else
#    pragma GCC diagnostic ignored "-Wattributes"
#  endif
#endif
