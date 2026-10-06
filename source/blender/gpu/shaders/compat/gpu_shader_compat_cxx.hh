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

#include <cstdio>  // IWYU pragma: export printf

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

/* Reserved keywords in GLSL that are allowed in preprocessor directives for compiling in C++. */
#define sizeof static_assert(false, "sizeof is a reserved keyword")

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
 * WORKAROUND(fclem): Only used for cases when passing down the resource_table is impractical.
 * Note that this placeholder is just for the code to compile.
 */
#define resource_table_get(table_type) (*(table_type *)(new char[1024 * 16]))

struct ShaderCreateInfo {};

struct NoConstants {};

template<typename VertFn,
         typename FragFn,
         typename ConstT1 = NoConstants,
         typename ConstT2 = NoConstants,
         typename ConstT3 = NoConstants,
         typename ConstT4 = NoConstants>
struct PipelineGraphic {
  VertFn vert;
  FragFn frag;
  /* Constant values. */
  ConstT1 c1;
  ConstT2 c2;
  ConstT3 c3;
  ConstT4 c4;

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
  PipelineGraphic(VertFn vert, FragFn frag, ConstT1 c1, ConstT2 c2, ConstT3 c3, ConstT4 c4)
      : vert(vert), frag(frag), c1(c1), c2(c2), c3(c3), c4(c4)
  {
  }
};

/* For assert support. */
#if defined(GPU_VERTEX_SHADER)
#  define GPU_THREAD uint3(0)
#elif defined(GPU_FRAGMENT_SHADER)
#  define GPU_THREAD uint3(0)
#elif defined(GPU_COMPUTE_SHADER)
#  define GPU_THREAD uint3(0)
#else
#  define GPU_THREAD error_not_in_a_shader_question_mark
#endif

template<typename CompFn,
         typename ConstT1 = NoConstants,
         typename ConstT2 = NoConstants,
         typename ConstT3 = NoConstants>
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

#ifndef FLT_MAX
#  define FLT_MAX uintBitsToFloat(0x7F7FFFFFu)
#  define FLT_MIN uintBitsToFloat(0x00800000u)
#  define FLT_EPSILON 1.192092896e-07F
#endif
#ifndef SHRT_MAX
#  define SHRT_MAX 0x00007FFF
#  define INT_MAX 0x7FFFFFFF
#  define USHRT_MAX 0x0000FFFFu
#  define UINT_MAX 0xFFFFFFFFu
#endif
#ifndef NAN_FLT
#  define NAN_FLT uintBitsToFloat(0x7FC00000u)
#endif

#include "GPU_shader_shared_utils.hh"

/* -------------------------------------------------------------------- */
/** \name Enums
 *
 * Enums should be defined in the root namespace when used directly in the pipeline, as they will
 * not be fully qualified when generating the template name substitution. Defining in the root
 * works around this limitation
 *
 * \{ */

/**
 * TextureWriteFormat.
 *
 * We can not use GPU_TEXTURE_WRITE_FORMAT_EXPAND as other parts are included that will intervene
 * with the compatibility defines.
 */
enum TextureWriteFormat : uint32_t {
  SNORM_8 = 0,
  SNORM_8_8,
  SNORM_8_8_8_8,

  SNORM_16,
  SNORM_16_16,
  SNORM_16_16_16_16,

  UNORM_8,
  UNORM_8_8,
  UNORM_8_8_8_8,

  UNORM_16,
  UNORM_16_16,
  UNORM_16_16_16_16,

  SINT_8,
  SINT_8_8,
  SINT_8_8_8_8,

  SINT_16,
  SINT_16_16,
  SINT_16_16_16_16,

  SINT_32,
  SINT_32_32,
  SINT_32_32_32_32,

  UINT_8,
  UINT_8_8,
  UINT_8_8_8_8,

  UINT_16,
  UINT_16_16,
  UINT_16_16_16_16,

  UINT_32,
  UINT_32_32,
  UINT_32_32_32_32,

  SFLOAT_16,
  SFLOAT_16_16,
  SFLOAT_16_16_16_16,

  SFLOAT_32,
  SFLOAT_32_32,
  SFLOAT_32_32_32_32,

  UNORM_10_10_10_2,
  UINT_10_10_10_2,

  UFLOAT_11_11_10,
};

/** \} */
