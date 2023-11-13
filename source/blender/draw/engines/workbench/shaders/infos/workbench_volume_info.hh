/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Volume shader base
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_volume_common)
    .vertex_in(0, Type::VEC3, "pos")
    .fragment_out(0, Type::VEC4, "fragColor")
    .sampler(0, ImageType::DEPTH_2D, "depthBuffer")
    .sampler(1, ImageType::FLOAT_3D, "densityTexture")
    .push_constant(Type::INT, "samplesLen")
    .push_constant(Type::FLOAT, "noiseOfs")
    .push_constant(Type::FLOAT, "stepLength")
    .push_constant(Type::FLOAT, "densityScale")
    .vertex_source("workbench_volume_vert.glsl")
    .fragment_source("workbench_volume_frag.glsl");

GPU_SHADER_CREATE_INFO(workbench_volume)
    .define("WORKBENCH_NEXT")
    .sampler(6, ImageType::UINT_2D, "stencil_tx")
    .additional_info("workbench_volume_common", "draw_object_infos_new", "draw_view");
/** \} */

/* -------------------------------------------------------------------- */
/** \name Smoke variation
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_volume_smoke_common)
    .define("VOLUME_SMOKE")
    .sampler(2, ImageType::FLOAT_3D, "flameTexture")
    .sampler(3, ImageType::FLOAT_1D, "flameColorTexture")
    .additional_info("draw_resource_id_varying");

GPU_SHADER_CREATE_INFO(workbench_volume_object_common)
    .define("VOLUME_OBJECT")
    .push_constant(Type::MAT4, "volumeTextureToObject")
    /* FIXME(fclem): This overflow the push_constant limit. */
    .push_constant(Type::MAT4, "volumeObjectToTexture")
    .additional_info("draw_resource_id_varying");

GPU_SHADER_CREATE_INFO(workbench_volume_smoke)
    .additional_info("workbench_volume_smoke_common", "draw_modelmat_new");

GPU_SHADER_CREATE_INFO(workbench_volume_object)
    .additional_info("workbench_volume_object_common", "draw_volume_new");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Band variation
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_volume_coba)
    .define("USE_COBA")
    .sampler(4, ImageType::UINT_3D, "flagTexture")
    .sampler(5, ImageType::FLOAT_1D, "transferTexture")
    .push_constant(Type::BOOL, "showPhi")
    .push_constant(Type::BOOL, "showFlags")
    .push_constant(Type::BOOL, "showPressure")
    .push_constant(Type::FLOAT, "gridScale");

GPU_SHADER_CREATE_INFO(workbench_volume_no_coba)
    .sampler(4, ImageType::FLOAT_3D, "shadowTexture")
    .push_constant(Type::VEC3, "activeColor");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sampling variation
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_volume_linear).define("USE_TRILINEAR");
GPU_SHADER_CREATE_INFO(workbench_volume_cubic).define("USE_TRICUBIC");
GPU_SHADER_CREATE_INFO(workbench_volume_closest).define("USE_CLOSEST");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Slice variation
 * \{ */

GPU_SHADER_INTERFACE_INFO(workbench_volume_iface, "").smooth(Type::VEC3, "localPos");

GPU_SHADER_CREATE_INFO(workbench_volume_slice)
    .define("VOLUME_SLICE")
    .vertex_in(1, Type::VEC3, "uvs")
    .vertex_out(workbench_volume_iface)
    .push_constant(Type::INT, "sliceAxis") /* -1 is no slice. */
    .push_constant(Type::FLOAT, "slicePosition");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Variations Declaration
 * \{ */

#define WORKBENCH_VOLUME_SLICE_VARIATIONS(prefix, ...) \
  GPU_SHADER_CREATE_INFO(prefix##_slice) \
      .additional_info("workbench_volume_slice", __VA_ARGS__) \
      .do_static_compilation(true); \
  GPU_SHADER_CREATE_INFO(prefix##_no_slice) \
      .additional_info(__VA_ARGS__) \
      .do_static_compilation(true);

#define WORKBENCH_VOLUME_COBA_VARIATIONS(prefix, ...) \
  WORKBENCH_VOLUME_SLICE_VARIATIONS(prefix##_coba, "workbench_volume_coba", __VA_ARGS__) \
  WORKBENCH_VOLUME_SLICE_VARIATIONS(prefix##_no_coba, "workbench_volume_no_coba", __VA_ARGS__)

#define WORKBENCH_VOLUME_INTERP_VARIATIONS(prefix, ...) \
  WORKBENCH_VOLUME_COBA_VARIATIONS(prefix##_linear, "workbench_volume_linear", __VA_ARGS__) \
  WORKBENCH_VOLUME_COBA_VARIATIONS(prefix##_cubic, "workbench_volume_cubic", __VA_ARGS__) \
  WORKBENCH_VOLUME_COBA_VARIATIONS(prefix##_closest, "workbench_volume_closest", __VA_ARGS__)

#define WORKBENCH_VOLUME_SMOKE_VARIATIONS(prefix, ...) \
  WORKBENCH_VOLUME_INTERP_VARIATIONS(prefix##_smoke, "workbench_volume_smoke", __VA_ARGS__) \
  WORKBENCH_VOLUME_INTERP_VARIATIONS(prefix##_object, "workbench_volume_object", __VA_ARGS__)

WORKBENCH_VOLUME_SMOKE_VARIATIONS(workbench_volume, "workbench_volume")

/** \} */
