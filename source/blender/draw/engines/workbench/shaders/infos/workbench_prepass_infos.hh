/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once

#  include "BLI_utildefines_variadic.h"

#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"

#  include "workbench_shader_shared.hh"
#  define WORKBENCH_COLOR_MATERIAL
#  define WORKBENCH_COLOR_TEXTURE
#  define WORKBENCH_TEXTURE_IMAGE_ARRAY
#  define WORKBENCH_COLOR_MATERIAL
#  define WORKBENCH_COLOR_VERTEX
#  define WORKBENCH_LIGHTING_MATCAP

#  define CURVES_SHADER
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
ADDITIONAL_INFO(draw_curves)
ADDITIONAL_INFO(draw_curves_infos)
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

/* clang-format off */
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_flat_material_clip, drw_clipped, workbench_color_material, workbench_lighting_flat, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_flat_material_no_clip, workbench_color_material, workbench_lighting_flat, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_flat_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_flat, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_flat_texture_no_clip, workbench_color_texture, workbench_lighting_flat, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_flat_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_flat, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_flat_vertex_no_clip, workbench_color_vertex, workbench_lighting_flat, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_studio_material_clip, drw_clipped, workbench_color_material, workbench_lighting_studio, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_studio_material_no_clip, workbench_color_material, workbench_lighting_studio, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_studio_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_studio, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_studio_texture_no_clip, workbench_color_texture, workbench_lighting_studio, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_studio_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_studio, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_studio_vertex_no_clip, workbench_color_vertex, workbench_lighting_studio, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_matcap_material_clip, drw_clipped, workbench_color_material, workbench_lighting_matcap, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_matcap_material_no_clip, workbench_color_material, workbench_lighting_matcap, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_matcap_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_matcap, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_matcap_texture_no_clip, workbench_color_texture, workbench_lighting_matcap, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_matcap_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_matcap, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_transparent_matcap_vertex_no_clip, workbench_color_vertex, workbench_lighting_matcap, workbench_transparent_accum, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_flat_material_clip, drw_clipped, workbench_color_material, workbench_lighting_flat, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_flat_material_no_clip, workbench_color_material, workbench_lighting_flat, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_flat_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_flat, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_flat_texture_no_clip, workbench_color_texture, workbench_lighting_flat, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_flat_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_flat, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_flat_vertex_no_clip, workbench_color_vertex, workbench_lighting_flat, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_studio_material_clip, drw_clipped, workbench_color_material, workbench_lighting_studio, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_studio_material_no_clip, workbench_color_material, workbench_lighting_studio, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_studio_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_studio, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_studio_texture_no_clip, workbench_color_texture, workbench_lighting_studio, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_studio_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_studio, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_studio_vertex_no_clip, workbench_color_vertex, workbench_lighting_studio, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_matcap_material_clip, drw_clipped, workbench_color_material, workbench_lighting_matcap, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_matcap_material_no_clip, workbench_color_material, workbench_lighting_matcap, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_matcap_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_matcap, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_matcap_texture_no_clip, workbench_color_texture, workbench_lighting_matcap, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_matcap_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_matcap, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_mesh_opaque_matcap_vertex_no_clip, workbench_color_vertex, workbench_lighting_matcap, workbench_opaque, workbench_mesh, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_flat_material_clip, drw_clipped, workbench_color_material, workbench_lighting_flat, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_flat_material_no_clip, workbench_color_material, workbench_lighting_flat, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_flat_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_flat, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_flat_texture_no_clip, workbench_color_texture, workbench_lighting_flat, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_flat_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_flat, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_flat_vertex_no_clip, workbench_color_vertex, workbench_lighting_flat, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_studio_material_clip, drw_clipped, workbench_color_material, workbench_lighting_studio, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_studio_material_no_clip, workbench_color_material, workbench_lighting_studio, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_studio_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_studio, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_studio_texture_no_clip, workbench_color_texture, workbench_lighting_studio, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_studio_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_studio, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_studio_vertex_no_clip, workbench_color_vertex, workbench_lighting_studio, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_matcap_material_clip, drw_clipped, workbench_color_material, workbench_lighting_matcap, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_matcap_material_no_clip, workbench_color_material, workbench_lighting_matcap, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_matcap_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_matcap, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_matcap_texture_no_clip, workbench_color_texture, workbench_lighting_matcap, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_matcap_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_matcap, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_transparent_matcap_vertex_no_clip, workbench_color_vertex, workbench_lighting_matcap, workbench_transparent_accum, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_flat_material_clip, drw_clipped, workbench_color_material, workbench_lighting_flat, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_flat_material_no_clip, workbench_color_material, workbench_lighting_flat, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_flat_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_flat, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_flat_texture_no_clip, workbench_color_texture, workbench_lighting_flat, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_flat_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_flat, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_flat_vertex_no_clip, workbench_color_vertex, workbench_lighting_flat, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_studio_material_clip, drw_clipped, workbench_color_material, workbench_lighting_studio, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_studio_material_no_clip, workbench_color_material, workbench_lighting_studio, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_studio_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_studio, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_studio_texture_no_clip, workbench_color_texture, workbench_lighting_studio, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_studio_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_studio, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_studio_vertex_no_clip, workbench_color_vertex, workbench_lighting_studio, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_matcap_material_clip, drw_clipped, workbench_color_material, workbench_lighting_matcap, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_matcap_material_no_clip, workbench_color_material, workbench_lighting_matcap, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_matcap_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_matcap, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_matcap_texture_no_clip, workbench_color_texture, workbench_lighting_matcap, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_matcap_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_matcap, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_curves_opaque_matcap_vertex_no_clip, workbench_color_vertex, workbench_lighting_matcap, workbench_opaque, workbench_curves, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_flat_material_clip, drw_clipped, workbench_color_material, workbench_lighting_flat, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_flat_material_no_clip, workbench_color_material, workbench_lighting_flat, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_flat_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_flat, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_flat_texture_no_clip, workbench_color_texture, workbench_lighting_flat, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_flat_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_flat, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_flat_vertex_no_clip, workbench_color_vertex, workbench_lighting_flat, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_studio_material_clip, drw_clipped, workbench_color_material, workbench_lighting_studio, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_studio_material_no_clip, workbench_color_material, workbench_lighting_studio, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_studio_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_studio, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_studio_texture_no_clip, workbench_color_texture, workbench_lighting_studio, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_studio_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_studio, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_studio_vertex_no_clip, workbench_color_vertex, workbench_lighting_studio, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_matcap_material_clip, drw_clipped, workbench_color_material, workbench_lighting_matcap, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_matcap_material_no_clip, workbench_color_material, workbench_lighting_matcap, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_matcap_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_matcap, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_matcap_texture_no_clip, workbench_color_texture, workbench_lighting_matcap, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_matcap_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_matcap, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_transparent_matcap_vertex_no_clip, workbench_color_vertex, workbench_lighting_matcap, workbench_transparent_accum, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_flat_material_clip, drw_clipped, workbench_color_material, workbench_lighting_flat, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_flat_material_no_clip, workbench_color_material, workbench_lighting_flat, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_flat_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_flat, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_flat_texture_no_clip, workbench_color_texture, workbench_lighting_flat, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_flat_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_flat, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_flat_vertex_no_clip, workbench_color_vertex, workbench_lighting_flat, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_studio_material_clip, drw_clipped, workbench_color_material, workbench_lighting_studio, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_studio_material_no_clip, workbench_color_material, workbench_lighting_studio, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_studio_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_studio, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_studio_texture_no_clip, workbench_color_texture, workbench_lighting_studio, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_studio_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_studio, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_studio_vertex_no_clip, workbench_color_vertex, workbench_lighting_studio, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_matcap_material_clip, drw_clipped, workbench_color_material, workbench_lighting_matcap, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_matcap_material_no_clip, workbench_color_material, workbench_lighting_matcap, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_matcap_texture_clip, drw_clipped, workbench_color_texture, workbench_lighting_matcap, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_matcap_texture_no_clip, workbench_color_texture, workbench_lighting_matcap, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_matcap_vertex_clip, drw_clipped, workbench_color_vertex, workbench_lighting_matcap, workbench_opaque, workbench_pointcloud, workbench_prepass)
CREATE_INFO_VARIANT(workbench_prepass_ptcloud_opaque_matcap_vertex_no_clip, workbench_color_vertex, workbench_lighting_matcap, workbench_opaque, workbench_pointcloud, workbench_prepass)
/* clang-format on */

/** \} */
