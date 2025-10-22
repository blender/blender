/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "BLI_utildefines_variadic.h"

#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"

#  include "eevee_common_infos.hh"
#  include "eevee_shadow_shared.hh"
#  include "eevee_volume_infos.hh"

#  define CURVES_SHADER
#  define DRW_HAIR_INFO

#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO

#  define SHADOW_UPDATE_ATOMIC_RASTER
#  define MAT_TRANSPARENT

#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Surface Mesh Type
 * \{ */

/* Common interface */
GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_iface, interp)
/* World Position. */
SMOOTH(float3, P)
/* World Normal. */
SMOOTH(float3, N)
GPU_SHADER_NAMED_INTERFACE_END(interp)

GPU_SHADER_CREATE_INFO(eevee_geom_mesh)
TYPEDEF_SOURCE("eevee_defines.hh")
DEFINE("MAT_GEOM_MESH")
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float3, nor)
VERTEX_SOURCE("eevee_geom_mesh_vert.glsl")
VERTEX_OUT(eevee_surf_iface)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_pointcloud_iface, pointcloud_interp)
SMOOTH(float, radius)
SMOOTH(float3, position)
GPU_SHADER_NAMED_INTERFACE_END(pointcloud_interp)
GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_pointcloud_flat_iface, pointcloud_interp_flat)
FLAT(int, id)
GPU_SHADER_NAMED_INTERFACE_END(pointcloud_interp_flat)

GPU_SHADER_CREATE_INFO(eevee_geom_pointcloud)
TYPEDEF_SOURCE("eevee_defines.hh")
PUSH_CONSTANT(bool, ptcloud_backface)
DEFINE("MAT_GEOM_POINTCLOUD")
VERTEX_SOURCE("eevee_geom_pointcloud_vert.glsl")
VERTEX_OUT(eevee_surf_iface)
VERTEX_OUT(eevee_surf_pointcloud_iface)
VERTEX_OUT(eevee_surf_pointcloud_flat_iface)
ADDITIONAL_INFO(draw_pointcloud)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_geom_volume)
TYPEDEF_SOURCE("eevee_defines.hh")
DEFINE("MAT_GEOM_VOLUME")
VERTEX_IN(0, float3, pos)
VERTEX_OUT(eevee_surf_iface)
VERTEX_SOURCE("eevee_geom_volume_vert.glsl")
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_volume_infos)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_curve_iface, curve_interp)
SMOOTH(float3, tangent)
SMOOTH(float3, binormal)
SMOOTH(float, time)
SMOOTH(float, time_width)
SMOOTH(float, radius)
SMOOTH(float, point_id) /* Smooth to be used for barycentric. */
GPU_SHADER_NAMED_INTERFACE_END(curve_interp)
GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_curve_flat_iface, curve_interp_flat)
FLAT(int, strand_id)
GPU_SHADER_NAMED_INTERFACE_END(curve_interp_flat)

GPU_SHADER_CREATE_INFO(eevee_geom_curves)
TYPEDEF_SOURCE("eevee_defines.hh")
DEFINE("MAT_GEOM_CURVES")
VERTEX_SOURCE("eevee_geom_curves_vert.glsl")
VERTEX_OUT(eevee_surf_iface)
VERTEX_OUT(eevee_surf_curve_iface)
VERTEX_OUT(eevee_surf_curve_flat_iface)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_curves)
ADDITIONAL_INFO(draw_curves_infos)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_geom_world)
TYPEDEF_SOURCE("eevee_defines.hh")
DEFINE("MAT_GEOM_WORLD")
BUILTINS(BuiltinBits::VERTEX_ID)
VERTEX_SOURCE("eevee_geom_world_vert.glsl")
VERTEX_OUT(eevee_surf_iface)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_object_infos) /* Unused, but allow debug compilation. */
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_surf_deferred_base)
DEFINE("MAT_DEFERRED")
DEFINE("GBUFFER_WRITE")
/* NOTE: This removes the possibility of using gl_FragDepth. */
EARLY_FRAGMENT_TEST(true)
/* Direct output. (Emissive, Holdout) */
FRAGMENT_OUT(0, float4, out_radiance)
FRAGMENT_OUT_ROG(1, uint, out_gbuf_header, DEFERRED_GBUFFER_ROG_ID)
FRAGMENT_OUT(2, float2, out_gbuf_normal)
FRAGMENT_OUT(3, float4, out_gbuf_closure1)
FRAGMENT_OUT(4, float4, out_gbuf_closure2)
/* Everything is stored inside a two layered target, one for each format. This is to fit the
 * limitation of the number of images we can bind on a single shader. */
IMAGE_FREQ(GBUF_CLOSURE_SLOT, UNORM_10_10_10_2, write, image2DArray, out_gbuf_closure_img, PASS)
IMAGE_FREQ(GBUF_NORMAL_SLOT, UNORM_16_16, write, image2DArray, out_gbuf_normal_img, PASS)
/* Storage for additional infos that are shared across closures. */
IMAGE_FREQ(GBUF_HEADER_SLOT, UINT_32, write, uimage2DArray, out_gbuf_header_img, PASS)
/* Added at runtime because of test shaders not having `node_tree`. */
// ADDITIONAL_INFO(eevee_render_pass_out)
// ADDITIONAL_INFO(eevee_cryptomatte_out)
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_utility_texture)
ADDITIONAL_INFO(eevee_sampling_data)
ADDITIONAL_INFO(eevee_hiz_data)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_deferred)
FRAGMENT_SOURCE("eevee_surf_deferred_frag.glsl")
ADDITIONAL_INFO(eevee_surf_deferred_base)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_deferred_hybrid)
FRAGMENT_SOURCE("eevee_surf_hybrid_frag.glsl")
ADDITIONAL_INFO(eevee_surf_deferred_base)
ADDITIONAL_INFO(eevee_light_data)
ADDITIONAL_INFO(eevee_lightprobe_data)
ADDITIONAL_INFO(eevee_shadow_data)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_forward)
DEFINE("MAT_FORWARD")
/* Early fragment test is needed for render passes support for forward surfaces. */
/* NOTE: This removes the possibility of using gl_FragDepth. */
EARLY_FRAGMENT_TEST(true)
FRAGMENT_OUT_DUAL(0, float4, out_radiance, SRC_0)
FRAGMENT_OUT_DUAL(0, float4, out_transmittance, SRC_1)
FRAGMENT_SOURCE("eevee_surf_forward_frag.glsl")
/* Optionally added depending on the material. */
//  ADDITIONAL_INFO(eevee_render_pass_out)
//  ADDITIONAL_INFO(eevee_cryptomatte_out)
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_light_data)
ADDITIONAL_INFO(eevee_lightprobe_data)
ADDITIONAL_INFO(eevee_utility_texture)
ADDITIONAL_INFO(eevee_sampling_data)
ADDITIONAL_INFO(eevee_shadow_data)
ADDITIONAL_INFO(eevee_hiz_data)
ADDITIONAL_INFO(eevee_volume_lib)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_capture)
DEFINE("MAT_CAPTURE")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
STORAGE_BUF(SURFEL_BUF_SLOT, write, Surfel, surfel_buf[])
STORAGE_BUF(CAPTURE_BUF_SLOT, read_write, CaptureInfoData, capture_info_buf)
PUSH_CONSTANT(bool, is_double_sided)
FRAGMENT_SOURCE("eevee_surf_capture_frag.glsl")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_utility_texture)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_depth)
DEFINE("MAT_DEPTH")
FRAGMENT_SOURCE("eevee_surf_depth_frag.glsl")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_sampling_data)
ADDITIONAL_INFO(eevee_utility_texture)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_world)
PUSH_CONSTANT(float, world_opacity_fade)
PUSH_CONSTANT(float, world_background_blur)
PUSH_CONSTANT(int4, world_coord_packed)
EARLY_FRAGMENT_TEST(true)
FRAGMENT_OUT(0, float4, out_background)
FRAGMENT_SOURCE("eevee_surf_world_frag.glsl")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_lightprobe_sphere_data)
ADDITIONAL_INFO(eevee_volume_probe_data)
ADDITIONAL_INFO(eevee_sampling_data)
/* Optionally added depending on the material. */
// ADDITIONAL_INFO(eevee_render_pass_out)
// ADDITIONAL_INFO(eevee_cryptomatte_out)
ADDITIONAL_INFO(eevee_utility_texture)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_renderpass_clear)
FRAGMENT_OUT(0, float4, out_background)
FRAGMENT_SOURCE("eevee_renderpass_clear_frag.glsl")
ADDITIONAL_INFO(gpu_fullscreen)
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_render_pass_out)
ADDITIONAL_INFO(eevee_cryptomatte_out)
TYPEDEF_SOURCE("eevee_defines.hh")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_shadow_atomic_iface, shadow_iface)
FLAT(int, shadow_view_id)
GPU_SHADER_NAMED_INTERFACE_END(shadow_iface)

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_shadow_clipping_iface, shadow_clip)
SMOOTH(float3, position)
SMOOTH(float3, vector)
GPU_SHADER_NAMED_INTERFACE_END(shadow_clip)

GPU_SHADER_CREATE_INFO(eevee_surf_shadow)
DEFINE_VALUE("DRW_VIEW_LEN", STRINGIFY(SHADOW_VIEW_MAX))
DEFINE("MAT_SHADOW")
TYPEDEF_SOURCE("eevee_shadow_shared.hh")
BUILTINS(BuiltinBits::VIEWPORT_INDEX)
VERTEX_OUT(eevee_surf_shadow_clipping_iface)
STORAGE_BUF(SHADOW_RENDER_VIEW_BUF_SLOT, read, ShadowRenderView, render_view_buf[SHADOW_VIEW_MAX])
FRAGMENT_SOURCE("eevee_surf_shadow_frag.glsl")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_utility_texture)
ADDITIONAL_INFO(eevee_sampling_data)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_shadow_atomic)
ADDITIONAL_INFO(eevee_surf_shadow)
DEFINE("SHADOW_UPDATE_ATOMIC_RASTER")
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
VERTEX_OUT(eevee_surf_shadow_atomic_iface)
STORAGE_BUF(SHADOW_RENDER_MAP_BUF_SLOT, read, uint, render_map_buf[SHADOW_RENDER_MAP_SIZE])
IMAGE(SHADOW_ATLAS_IMG_SLOT, UINT_32, read_write, uimage2DArrayAtomic, shadow_atlas_img)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_shadow_tbdr)
ADDITIONAL_INFO(eevee_surf_shadow)
DEFINE("SHADOW_UPDATE_TBDR")
BUILTINS(BuiltinBits::LAYER)
/* Use greater depth write to avoid loosing the early Z depth test but ensure correct fragment
 * ordering after slope bias. */
DEPTH_WRITE(DepthWrite::GREATER)
/* F32 color attachment for on-tile depth accumulation without atomics. */
FRAGMENT_OUT_ROG(0, float, out_depth, SHADOW_ROG_ID)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_surf_volume)
DEFINE("MAT_VOLUME")
/* Only the front fragments have to be invoked. */
EARLY_FRAGMENT_TEST(true)
IMAGE(VOLUME_PROP_SCATTERING_IMG_SLOT, UFLOAT_11_11_10, read_write, image3D, out_scattering_img)
IMAGE(VOLUME_PROP_EXTINCTION_IMG_SLOT, UFLOAT_11_11_10, read_write, image3D, out_extinction_img)
IMAGE(VOLUME_PROP_EMISSION_IMG_SLOT, UFLOAT_11_11_10, read_write, image3D, out_emissive_img)
IMAGE(VOLUME_PROP_PHASE_IMG_SLOT, SFLOAT_16, read_write, image3D, out_phase_img)
IMAGE(VOLUME_PROP_PHASE_WEIGHT_IMG_SLOT, SFLOAT_16, read_write, image3D, out_phase_weight_img)
IMAGE(VOLUME_OCCUPANCY_SLOT, UINT_32, read, uimage3DAtomic, occupancy_img)
FRAGMENT_SOURCE("eevee_surf_volume_frag.glsl")
ADDITIONAL_INFO(draw_modelmat_common)
ADDITIONAL_INFO(draw_view)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_sampling_data)
ADDITIONAL_INFO(eevee_utility_texture)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_occupancy)
DEFINE("MAT_OCCUPANCY")
/* All fragments need to be invoked even if we write to the depth buffer. */
EARLY_FRAGMENT_TEST(false)
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
PUSH_CONSTANT(bool, use_fast_method)
IMAGE(VOLUME_HIT_DEPTH_SLOT, SFLOAT_32, write, image3D, hit_depth_img)
IMAGE(VOLUME_HIT_COUNT_SLOT, UINT_32, read_write, uimage2DAtomic, hit_count_img)
IMAGE(VOLUME_OCCUPANCY_SLOT, UINT_32, read_write, uimage3DAtomic, occupancy_img)
FRAGMENT_SOURCE("eevee_surf_occupancy_frag.glsl")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_sampling_data)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test shaders
 *
 * Variations that are only there to test shaders at compile time.
 * \{ */

#ifndef NDEBUG

GPU_SHADER_CREATE_INFO(eevee_material_stub)
/* Dummy uniform buffer to detect overlap with material node-tree. */
UNIFORM_BUF(0, int, node_tree)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(eevee_surface_world_world, eevee_geom_world, eevee_surf_world, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_world_curves, eevee_geom_curves, eevee_surf_world, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_world_mesh, eevee_geom_mesh, eevee_surf_world, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_world_pointcloud, eevee_geom_pointcloud, eevee_surf_world, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_world_volume, eevee_geom_volume, eevee_surf_world, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_depth_world, eevee_geom_world, eevee_surf_depth, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_depth_curves, eevee_geom_curves, eevee_surf_depth, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_depth_mesh, eevee_geom_mesh, eevee_surf_depth, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_depth_pointcloud, eevee_geom_pointcloud, eevee_surf_depth, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_depth_volume, eevee_geom_volume, eevee_surf_depth, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_deferred_world, eevee_geom_world, eevee_surf_deferred, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_deferred_curves, eevee_geom_curves, eevee_surf_deferred, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_deferred_mesh, eevee_geom_mesh, eevee_surf_deferred, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_deferred_pointcloud, eevee_geom_pointcloud, eevee_surf_deferred, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_deferred_volume, eevee_geom_volume, eevee_surf_deferred, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_forward_world, eevee_geom_world, eevee_surf_forward, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_forward_curves, eevee_geom_curves, eevee_surf_forward, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_forward_mesh, eevee_geom_mesh, eevee_surf_forward, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_forward_pointcloud, eevee_geom_pointcloud, eevee_surf_forward, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_forward_volume, eevee_geom_volume, eevee_surf_forward, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_capture_world, eevee_geom_world, eevee_surf_capture, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_capture_curves, eevee_geom_curves, eevee_surf_capture, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_capture_mesh, eevee_geom_mesh, eevee_surf_capture, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_capture_pointcloud, eevee_geom_pointcloud, eevee_surf_capture, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_capture_volume, eevee_geom_volume, eevee_surf_capture, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_volume_world, eevee_geom_world, eevee_surf_volume, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_volume_curves, eevee_geom_curves, eevee_surf_volume, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_volume_mesh, eevee_geom_mesh, eevee_surf_volume, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_volume_pointcloud, eevee_geom_pointcloud, eevee_surf_volume, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_volume_volume, eevee_geom_volume, eevee_surf_volume, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_occupancy_world, eevee_geom_world, eevee_surf_occupancy, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_occupancy_curves, eevee_geom_curves, eevee_surf_occupancy, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_occupancy_mesh, eevee_geom_mesh, eevee_surf_occupancy, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_occupancy_pointcloud, eevee_geom_pointcloud, eevee_surf_occupancy, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_occupancy_volume, eevee_geom_volume, eevee_surf_occupancy, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_atomic_world, eevee_geom_world, eevee_surf_shadow_atomic, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_atomic_curves, eevee_geom_curves, eevee_surf_shadow_atomic, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_atomic_mesh, eevee_geom_mesh, eevee_surf_shadow_atomic, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_atomic_pointcloud, eevee_geom_pointcloud, eevee_surf_shadow_atomic, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_atomic_volume, eevee_geom_volume, eevee_surf_shadow_atomic, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_tbdr_world, eevee_geom_world, eevee_surf_shadow_tbdr, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_tbdr_curves, eevee_geom_curves, eevee_surf_shadow_tbdr, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_tbdr_mesh, eevee_geom_mesh, eevee_surf_shadow_tbdr, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_tbdr_pointcloud, eevee_geom_pointcloud, eevee_surf_shadow_tbdr, eevee_material_stub)
CREATE_INFO_VARIANT(eevee_surface_shadow_tbdr_volume, eevee_geom_volume, eevee_surf_shadow_tbdr, eevee_material_stub)
/* clang-format on */
#endif

/** \} */
