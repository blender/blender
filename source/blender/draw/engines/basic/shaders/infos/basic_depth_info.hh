/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Conservative Rasterization
 *
 * Allow selection of sub-pixel objects.
 * \{ */

GPU_SHADER_CREATE_INFO(basic_conservative)
GEOMETRY_LAYOUT(PrimitiveIn::TRIANGLES, PrimitiveOut::TRIANGLE_STRIP, 3)
GEOMETRY_SOURCE("basic_conservative_depth_geom.glsl")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object types
 * \{ */

GPU_SHADER_CREATE_INFO(basic_mesh)
VERTEX_IN(0, VEC3, pos)
VERTEX_SOURCE("basic_depth_vert.glsl")
ADDITIONAL_INFO(draw_mesh)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(basic_pointcloud)
VERTEX_SOURCE("basic_depth_pointcloud_vert.glsl")
ADDITIONAL_INFO(draw_pointcloud)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(basic_curves)
VERTEX_SOURCE("basic_depth_curves_vert.glsl")
ADDITIONAL_INFO(draw_hair)
GPU_SHADER_CREATE_END()

/* Geometry-shader alternative paths. */
GPU_SHADER_CREATE_INFO(basic_mesh_conservative_no_geom)
VERTEX_IN(0, VEC3, pos)
VERTEX_SOURCE("basic_depth_vert_conservative_no_geom.glsl")
ADDITIONAL_INFO(draw_mesh)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(basic_pointcloud_conservative_no_geom)
DEFINE("CONSERVATIVE_RASTER")
VERTEX_SOURCE("basic_depth_pointcloud_vert.glsl")
ADDITIONAL_INFO(draw_pointcloud)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Variations Declaration
 * \{ */

#define BASIC_CLIPPING_VARIATIONS(prefix, ...) \
  CREATE_INFO_VARIANT(prefix##_clipped, drw_clipped, __VA_ARGS__) \
  CREATE_INFO_VARIANT(prefix, __VA_ARGS__)

#define BASIC_CONSERVATIVE_VARIATIONS(prefix, ...) \
  BASIC_CLIPPING_VARIATIONS(prefix##_conservative, basic_conservative, __VA_ARGS__) \
  BASIC_CLIPPING_VARIATIONS(prefix##_conservative_no_geom, __VA_ARGS__) \
  BASIC_CLIPPING_VARIATIONS(prefix, __VA_ARGS__)

#define BASIC_OBTYPE_VARIATIONS(prefix, ...) \
  BASIC_CONSERVATIVE_VARIATIONS(prefix##_mesh, basic_mesh, __VA_ARGS__) \
  BASIC_CONSERVATIVE_VARIATIONS(prefix##_pointcloud, basic_pointcloud, __VA_ARGS__) \
  BASIC_CLIPPING_VARIATIONS(prefix##_curves, basic_curves, __VA_ARGS__)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Depth shader types.
 * \{ */

GPU_SHADER_CREATE_INFO(basic_depth)
FRAGMENT_SOURCE("basic_depth_frag.glsl")
GPU_SHADER_CREATE_END()

BASIC_OBTYPE_VARIATIONS(basic_depth, basic_depth, draw_globals)

/** \} */
