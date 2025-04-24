/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once

#  include "BLI_utildefines_variadic.h"

#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_object_infos_info.hh"
#  include "draw_view_info.hh"

#  include "workbench_shader_shared.hh"
#  define WORKBENCH_COLOR_MATERIAL
#  define WORKBENCH_COLOR_TEXTURE
#  define WORKBENCH_TEXTURE_IMAGE_ARRAY
#  define WORKBENCH_COLOR_MATERIAL
#  define WORKBENCH_COLOR_VERTEX
#  define WORKBENCH_LIGHTING_MATCAP

#  define HAIR_SHADER
#  define DRW_HAIR_INFO

#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO
#endif

#include "gpu_shader_create_info.hh"
#include "workbench_defines.hh"

/* -------------------------------------------------------------------- */
/** \name Object Type
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_mesh)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float3, nor)
VERTEX_IN(2, float4, ac)
VERTEX_IN(3, float2, au)
VERTEX_SOURCE("workbench_prepass_vert.glsl")
ADDITIONAL_INFO(draw_modelmat_with_custom_id)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_curves)
SAMPLER_FREQ(WB_CURVES_COLOR_SLOT, samplerBuffer, ac, BATCH)
SAMPLER_FREQ(WB_CURVES_UV_SLOT, samplerBuffer, au, BATCH)
PUSH_CONSTANT(int, emitter_object_id)
VERTEX_SOURCE("workbench_prepass_hair_vert.glsl")
ADDITIONAL_INFO(draw_modelmat_with_custom_id)
ADDITIONAL_INFO(draw_hair)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_pointcloud)
VERTEX_SOURCE("workbench_prepass_pointcloud_vert.glsl")
ADDITIONAL_INFO(draw_modelmat_with_custom_id)
ADDITIONAL_INFO(draw_pointcloud)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lighting Type (only for transparent)
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_lighting_flat)
DEFINE("WORKBENCH_LIGHTING_FLAT")
GPU_SHADER_CREATE_END()
GPU_SHADER_CREATE_INFO(workbench_lighting_studio)
DEFINE("WORKBENCH_LIGHTING_STUDIO")
GPU_SHADER_CREATE_END()
GPU_SHADER_CREATE_INFO(workbench_lighting_matcap)
DEFINE("WORKBENCH_LIGHTING_MATCAP")
SAMPLER(WB_MATCAP_SLOT, sampler2DArray, matcap_tx)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Interface
 * \{ */

GPU_SHADER_INTERFACE_INFO(workbench_material_iface)
SMOOTH(float3, normal_interp)
SMOOTH(float3, color_interp)
SMOOTH(float, alpha_interp)
SMOOTH(float2, uv_interp)
FLAT(int, object_id)
FLAT(float, _roughness)
FLAT(float, metallic)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(workbench_color_material)
DEFINE("WORKBENCH_COLOR_MATERIAL")
STORAGE_BUF(WB_MATERIAL_SLOT, read, float4, materials_data[])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_color_texture)
DEFINE("WORKBENCH_COLOR_TEXTURE")
DEFINE("WORKBENCH_TEXTURE_IMAGE_ARRAY")
DEFINE("WORKBENCH_COLOR_MATERIAL")
STORAGE_BUF(WB_MATERIAL_SLOT, read, float4, materials_data[])
SAMPLER_FREQ(WB_TEXTURE_SLOT, sampler2D, imageTexture, BATCH)
SAMPLER_FREQ(WB_TILE_ARRAY_SLOT, sampler2DArray, imageTileArray, BATCH)
SAMPLER_FREQ(WB_TILE_DATA_SLOT, sampler1DArray, imageTileData, BATCH)
PUSH_CONSTANT(bool, is_image_tile)
PUSH_CONSTANT(bool, image_premult)
PUSH_CONSTANT(float, image_transparency_cutoff)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_color_vertex)
DEFINE("WORKBENCH_COLOR_VERTEX")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_prepass)
UNIFORM_BUF(WB_WORLD_SLOT, WorldData, world_data)
VERTEX_OUT(workbench_material_iface)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pipeline Type
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_transparent_accum)
/* NOTE: Blending will be skipped on objectId because output is a
 * non-normalized integer buffer. */
FRAGMENT_OUT(0, float4, out_transparent_accum)
FRAGMENT_OUT(1, float4, out_revealage_accum)
FRAGMENT_OUT(2, uint, out_object_id)
PUSH_CONSTANT(bool, force_shadowing)
TYPEDEF_SOURCE("workbench_shader_shared.hh")
FRAGMENT_SOURCE("workbench_transparent_accum_frag.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_opaque)
FRAGMENT_OUT(0, float4, out_material)
FRAGMENT_OUT(1, float2, out_normal)
FRAGMENT_OUT(2, uint, out_object_id)
TYPEDEF_SOURCE("workbench_shader_shared.hh")
FRAGMENT_SOURCE("workbench_prepass_frag.glsl")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Variations Declaration
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_flat)
DEFINE("WORKBENCH_SHADING_FLAT")
GPU_SHADER_CREATE_END()
GPU_SHADER_CREATE_INFO(workbench_studio)
DEFINE("WORKBENCH_SHADING_STUDIO")
GPU_SHADER_CREATE_END()
GPU_SHADER_CREATE_INFO(workbench_matcap)
DEFINE("WORKBENCH_SHADING_MATCAP")
GPU_SHADER_CREATE_END()

#define WORKBENCH_CLIPPING_VARIATIONS(prefix, ...) \
  CREATE_INFO_VARIANT(prefix##_clip, drw_clipped, __VA_ARGS__) \
  CREATE_INFO_VARIANT(prefix##_no_clip, __VA_ARGS__)

#define WORKBENCH_COLOR_VARIATIONS(prefix, ...) \
  WORKBENCH_CLIPPING_VARIATIONS(prefix##_material, workbench_color_material, __VA_ARGS__) \
  WORKBENCH_CLIPPING_VARIATIONS(prefix##_texture, workbench_color_texture, __VA_ARGS__) \
  WORKBENCH_CLIPPING_VARIATIONS(prefix##_vertex, workbench_color_vertex, __VA_ARGS__)

#define WORKBENCH_SHADING_VARIATIONS(prefix, ...) \
  WORKBENCH_COLOR_VARIATIONS(prefix##_flat, workbench_lighting_flat, __VA_ARGS__) \
  WORKBENCH_COLOR_VARIATIONS(prefix##_studio, workbench_lighting_studio, __VA_ARGS__) \
  WORKBENCH_COLOR_VARIATIONS(prefix##_matcap, workbench_lighting_matcap, __VA_ARGS__)

#define WORKBENCH_PIPELINE_VARIATIONS(prefix, ...) \
  WORKBENCH_SHADING_VARIATIONS(prefix##_transparent, workbench_transparent_accum, __VA_ARGS__) \
  WORKBENCH_SHADING_VARIATIONS(prefix##_opaque, workbench_opaque, __VA_ARGS__)

#define WORKBENCH_GEOMETRY_VARIATIONS(prefix, ...) \
  WORKBENCH_PIPELINE_VARIATIONS(prefix##_mesh, workbench_mesh, __VA_ARGS__) \
  WORKBENCH_PIPELINE_VARIATIONS(prefix##_curves, workbench_curves, __VA_ARGS__) \
  WORKBENCH_PIPELINE_VARIATIONS(prefix##_ptcloud, workbench_pointcloud, __VA_ARGS__)

WORKBENCH_GEOMETRY_VARIATIONS(workbench_prepass, workbench_prepass)

#undef WORKBENCH_FINAL_VARIATION
#undef WORKBENCH_CLIPPING_VARIATIONS
#undef WORKBENCH_TEXTURE_VARIATIONS
#undef WORKBENCH_DATATYPE_VARIATIONS
#undef WORKBENCH_PIPELINE_VARIATIONS

/** \} */
