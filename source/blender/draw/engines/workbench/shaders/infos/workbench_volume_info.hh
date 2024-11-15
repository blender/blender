/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "BLI_utildefines_variadic.h"

#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_object_infos_info.hh"

#  include "workbench_shader_shared.h"
#  define VOLUME_SMOKE
#  define VOLUME_OBJECT
#  define USE_COBA
#  define USE_TRILINEAR
#  define USE_TRICUBIC
#  define USE_CLOSEST
#  define VOLUME_SLICE
#endif

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Volume shader base
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_volume_common)
VERTEX_IN(0, VEC3, pos)
FRAGMENT_OUT(0, VEC4, fragColor)
SAMPLER(0, DEPTH_2D, depthBuffer)
SAMPLER(1, FLOAT_3D, densityTexture)
PUSH_CONSTANT(INT, samplesLen)
PUSH_CONSTANT(FLOAT, noiseOfs)
PUSH_CONSTANT(FLOAT, stepLength)
PUSH_CONSTANT(FLOAT, densityScale)
PUSH_CONSTANT(BOOL, do_depth_test)
VERTEX_SOURCE("workbench_volume_vert.glsl")
FRAGMENT_SOURCE("workbench_volume_frag.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_volume)
SAMPLER(6, UINT_2D, stencil_tx)
ADDITIONAL_INFO(workbench_volume_common)
ADDITIONAL_INFO(draw_object_infos_new)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()
/** \} */

/* -------------------------------------------------------------------- */
/** \name Smoke variation
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_volume_smoke_common)
DEFINE("VOLUME_SMOKE")
SAMPLER(2, FLOAT_3D, flameTexture)
SAMPLER(3, FLOAT_1D, flameColorTexture)
ADDITIONAL_INFO(draw_resource_id_varying)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_volume_object_common)
DEFINE("VOLUME_OBJECT")
PUSH_CONSTANT(MAT4, volumeTextureToObject)
/* FIXME(fclem): This overflow the push_constant limit. */
PUSH_CONSTANT(MAT4, volumeObjectToTexture)
ADDITIONAL_INFO(draw_resource_id_varying)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_volume_smoke)
ADDITIONAL_INFO(workbench_volume_smoke_common)
ADDITIONAL_INFO(draw_modelmat_new)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_volume_object)
ADDITIONAL_INFO(workbench_volume_object_common)
ADDITIONAL_INFO(draw_volume_new)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Band variation
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_volume_coba)
DEFINE("USE_COBA")
SAMPLER(4, UINT_3D, flagTexture)
SAMPLER(5, FLOAT_1D, transferTexture)
PUSH_CONSTANT(BOOL, showPhi)
PUSH_CONSTANT(BOOL, showFlags)
PUSH_CONSTANT(BOOL, showPressure)
PUSH_CONSTANT(FLOAT, gridScale)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_volume_no_coba)
SAMPLER(4, FLOAT_3D, shadowTexture)
PUSH_CONSTANT(VEC3, activeColor)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sampling variation
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_volume_linear)
DEFINE("USE_TRILINEAR")
GPU_SHADER_CREATE_END()
GPU_SHADER_CREATE_INFO(workbench_volume_cubic)
DEFINE("USE_TRICUBIC")
GPU_SHADER_CREATE_END()
GPU_SHADER_CREATE_INFO(workbench_volume_closest)
DEFINE("USE_CLOSEST")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Slice variation
 * \{ */

GPU_SHADER_INTERFACE_INFO(workbench_volume_iface)
SMOOTH(VEC3, localPos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(workbench_volume_slice)
DEFINE("VOLUME_SLICE")
VERTEX_IN(1, VEC3, uvs)
VERTEX_OUT(workbench_volume_iface)
PUSH_CONSTANT(INT, sliceAxis) /* -1 is no slice. */
PUSH_CONSTANT(FLOAT, slicePosition)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Variations Declaration
 * \{ */

#define WORKBENCH_VOLUME_SLICE_VARIATIONS(prefix, ...) \
  CREATE_INFO_VARIANT(prefix##_slice, workbench_volume_slice, __VA_ARGS__) \
  CREATE_INFO_VARIANT(prefix##_no_slice, __VA_ARGS__)

#define WORKBENCH_VOLUME_COBA_VARIATIONS(prefix, ...) \
  WORKBENCH_VOLUME_SLICE_VARIATIONS(prefix##_coba, workbench_volume_coba, __VA_ARGS__) \
  WORKBENCH_VOLUME_SLICE_VARIATIONS(prefix##_no_coba, workbench_volume_no_coba, __VA_ARGS__)

#define WORKBENCH_VOLUME_INTERP_VARIATIONS(prefix, ...) \
  WORKBENCH_VOLUME_COBA_VARIATIONS(prefix##_linear, workbench_volume_linear, __VA_ARGS__) \
  WORKBENCH_VOLUME_COBA_VARIATIONS(prefix##_cubic, workbench_volume_cubic, __VA_ARGS__) \
  WORKBENCH_VOLUME_COBA_VARIATIONS(prefix##_closest, workbench_volume_closest, __VA_ARGS__)

#define WORKBENCH_VOLUME_SMOKE_VARIATIONS(prefix, ...) \
  WORKBENCH_VOLUME_INTERP_VARIATIONS(prefix##_smoke, workbench_volume_smoke, __VA_ARGS__) \
  WORKBENCH_VOLUME_INTERP_VARIATIONS(prefix##_object, workbench_volume_object, __VA_ARGS__)

WORKBENCH_VOLUME_SMOKE_VARIATIONS(workbench_volume, workbench_volume)

/** \} */
