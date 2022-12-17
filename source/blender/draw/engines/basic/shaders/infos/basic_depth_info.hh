/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Conservative Rasterization
 *
 * Allow selection of sub-pixel objects.
 * \{ */

GPU_SHADER_CREATE_INFO(basic_conservative)
    .geometry_layout(PrimitiveIn::TRIANGLES, PrimitiveOut::TRIANGLE_STRIP, 3)
    .geometry_source("basic_conservative_depth_geom.glsl");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object types
 * \{ */

GPU_SHADER_CREATE_INFO(basic_mesh)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_source("basic_depth_vert.glsl")
    .additional_info("draw_mesh");

GPU_SHADER_CREATE_INFO(basic_pointcloud)
    .vertex_source("basic_depth_pointcloud_vert.glsl")
    .additional_info("draw_pointcloud");

GPU_SHADER_CREATE_INFO(basic_curves)
    .vertex_source("basic_depth_curves_vert.glsl")
    .additional_info("draw_hair");

/* Geometry-shader alternative paths. */
GPU_SHADER_CREATE_INFO(basic_mesh_conservative_no_geom)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_source("basic_depth_vert_conservative_no_geom.glsl")
    .additional_info("draw_mesh");

GPU_SHADER_CREATE_INFO(basic_pointcloud_conservative_no_geom)
    .define("CONSERVATIVE_RASTER")
    .vertex_source("basic_depth_pointcloud_vert.glsl")
    .additional_info("draw_pointcloud");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Variations Declaration
 * \{ */

#define BASIC_FINAL_VARIATION(name, ...) \
  GPU_SHADER_CREATE_INFO(name).additional_info(__VA_ARGS__).do_static_compilation(true);

#define BASIC_CLIPPING_VARIATIONS(prefix, ...) \
  BASIC_FINAL_VARIATION(prefix##_clipped, "drw_clipped", __VA_ARGS__) \
  BASIC_FINAL_VARIATION(prefix, __VA_ARGS__)

#define BASIC_CONSERVATIVE_VARIATIONS(prefix, ...) \
  BASIC_CLIPPING_VARIATIONS(prefix##_conservative, "basic_conservative", __VA_ARGS__) \
  BASIC_CLIPPING_VARIATIONS(prefix##_conservative_no_geom, __VA_ARGS__) \
  BASIC_CLIPPING_VARIATIONS(prefix, __VA_ARGS__)

#define BASIC_OBTYPE_VARIATIONS(prefix, ...) \
  BASIC_CONSERVATIVE_VARIATIONS(prefix##_mesh, "basic_mesh", __VA_ARGS__) \
  BASIC_CONSERVATIVE_VARIATIONS(prefix##_pointcloud, "basic_pointcloud", __VA_ARGS__) \
  BASIC_CLIPPING_VARIATIONS(prefix##_curves, "basic_curves", __VA_ARGS__)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Depth shader types.
 * \{ */

GPU_SHADER_CREATE_INFO(basic_depth).fragment_source("basic_depth_frag.glsl");

BASIC_OBTYPE_VARIATIONS(basic_depth, "basic_depth", "draw_globals");

/** \} */
