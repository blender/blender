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

#include <stdio.h>  // printf

#include "gpu_shader_cxx_builtin.hh"  // IWYU pragma: export
#include "gpu_shader_cxx_global.hh"   // IWYU pragma: export
#include "gpu_shader_cxx_image.hh"    // IWYU pragma: export
#include "gpu_shader_cxx_matrix.hh"   // IWYU pragma: export
#include "gpu_shader_cxx_sampler.hh"  // IWYU pragma: export
#include "gpu_shader_cxx_string.hh"   // IWYU pragma: export
#include "gpu_shader_cxx_vector.hh"   // IWYU pragma: export

#define assert(assertion)

#include "gpu_shader_cxx_attribute.hh"  // IWYU pragma: export

/* -------------------------------------------------------------------- */
/** \name Keywords
 * \{ */

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

#define reserved_keyword(keyword) static_assert(false, keyword " is a reserved keyword")
/* List of reserved keywords in GLSL. */
#define common reserved_keyword("common")
#define partition reserved_keyword("partition")
#define active reserved_keyword("active")
// #define class /* Supported. */
// #define union /* Supported. */
// #define enum /* Supported. */
#define typedef reserved_keyword("typedef")
// #define template /* Needed for Stubs. */
// #define this /* Needed for Stubs. */
#define packed reserved_keyword("packed")
#define resource reserved_keyword("resource")
#define goto reserved_keyword("goto")
// #define inline  /* Supported. */
#define noinline reserved_keyword("noinline")
// #define public /* Supported. */
// #define private /* Supported. */
// #define static /* Supported. */
// #define extern /* Needed for Stubs. */
#define external reserved_keyword("external")
#define interface reserved_keyword("interface")
#define long reserved_keyword("long")
// #define short /* Supported. */
// #define half /* Supported. */
#define fixed reserved_keyword("fixed")
#define unsigned reserved_keyword("unsigned")
#define superp reserved_keyword("superp")
#define input reserved_keyword("input")
#define output reserved_keyword("output")
#define hvec2 reserved_keyword("hvec2")
#define hvec3 reserved_keyword("hvec3")
#define hvec4 reserved_keyword("hvec4")
#define fvec2 reserved_keyword("fvec2")
#define fvec3 reserved_keyword("fvec3")
#define fvec4 reserved_keyword("fvec4")
#define sampler3DRect reserved_keyword("sampler3DRect")
#define filter reserved_keyword("filter")
#define sizeof reserved_keyword("sizeof")
#define cast reserved_keyword("cast")
// #define namespace /* Needed for Stubs. */
// #define using /* Needed for Stubs. */
#define row_major reserved_keyword("row_major")
#define inout reserved_keyword("inout")

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
#define srt_access(create_info, _res) create_info::_res

/**
 * Member hiding type.
 * Allows to declare fake references to Shader Resource Tables.
 * This make sure we cannot directly reference them.
 * This is just a safety measure for our fragile SRT implementation which cannot safely directly
 * access SRT members that are more that 1 level deep.
 * This should only be used in SRT struct member declaration for wrapping other SRT types.
 */
template<typename T> struct srt_t {
  operator const T &() const
  {
    return *reinterpret_cast<const T *>(this);
  }

  operator T &()
  {
    return *reinterpret_cast<T *>(this);
  }
};

/**
 * Member hiding type.
 * Wrapper type for members of unions in host shared structure.
 * This is needed to force the accessor syntax in the shader code.
 */
template<typename T> struct union_t {
  const T &operator()() const
  {
    return *reinterpret_cast<const T *>(this);
  }

  T &operator()()
  {
    return *reinterpret_cast<T *>(this);
  }
};

struct ShaderCreateInfo {};

struct NoConstants {};

template<typename VertFn,
         typename FragFn,
         typename ConstT1 = NoConstants,
         typename ConstT2 = ConstT1,
         typename ConstT3 = ConstT2>
struct PipelineGraphic {
  VertFn vert;
  FragFn frag;
  /* Constant values. */
  ConstT1 c1;
  ConstT2 c2;
  ConstT3 c3;

  PipelineGraphic(VertFn vert, FragFn frag) : vert(vert), frag(frag), c1({}), c2({}), c3({}) {}
  PipelineGraphic(VertFn vert, FragFn frag, ConstT1 c1)
      : vert(vert), frag(frag), c1(c1), c2({}), c3({})
  {
  }
  PipelineGraphic(VertFn vert, FragFn frag, ConstT1 c1, ConstT2 c2)
      : vert(vert), frag(frag), c1(c1), c2(c2), c3({})
  {
  }
  PipelineGraphic(VertFn vert, FragFn frag, ConstT1 c1, ConstT2 c2, ConstT3 c3)
      : vert(vert), frag(frag), c1(c1), c2(c2), c3(c3)
  {
  }
};

template<typename CompFn,
         typename ConstT1 = NoConstants,
         typename ConstT2 = ConstT1,
         typename ConstT3 = ConstT2>
struct PipelineCompute {
  CompFn comp;
  /* Constant values. */
  ConstT1 c1;
  ConstT2 c2;
  ConstT3 c3;

  PipelineCompute(CompFn comp) : comp(comp), c1({}), c2({}), c3({}) {}
  PipelineCompute(CompFn comp, ConstT1 c1) : comp(comp), c1(c1), c2({}), c3({}) {}
  PipelineCompute(CompFn comp, ConstT1 c1, ConstT2 c2) : comp(comp), c1(c1), c2(c2), c3({}) {}
  PipelineCompute(CompFn comp, ConstT1 c1, ConstT2 c2, ConstT3 c3)
      : comp(comp), c1(c1), c2(c2), c3(c3)
  {
  }
};

#include "GPU_shader_shared_utils.hh"
