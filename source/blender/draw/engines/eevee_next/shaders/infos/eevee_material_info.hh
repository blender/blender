/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

/* TODO(@fclem): This is a bit out of place at the moment. */
GPU_SHADER_CREATE_INFO(eevee_shared)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_shader_shared.hh")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_global_ubo)
UNIFORM_BUF(UNIFORM_BUF_SLOT, UniformData, uniform_buf)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_sampling_data)
DEFINE("EEVEE_SAMPLING_DATA")
ADDITIONAL_INFO(eevee_shared)
STORAGE_BUF(SAMPLING_BUF_SLOT, READ, SamplingData, sampling_buf)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_utility_texture)
DEFINE("EEVEE_UTILITY_TX")
SAMPLER(RBUFS_UTILITY_TEX_SLOT, FLOAT_2D_ARRAY, utility_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_clip_plane_iface, clip_interp)
SMOOTH(FLOAT, clip_distance)
GPU_SHADER_NAMED_INTERFACE_END(clip_interp)

GPU_SHADER_CREATE_INFO(eevee_clip_plane)
VERTEX_OUT(eevee_clip_plane_iface)
UNIFORM_BUF(CLIP_PLANE_BUF, ClipPlaneData, clip_plane)
DEFINE("MAT_CLIP_PLANE")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface Mesh Type
 * \{ */

/* Common interface */
GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_iface, interp)
/* World Position. */
SMOOTH(VEC3, P)
/* World Normal. */
SMOOTH(VEC3, N)
GPU_SHADER_NAMED_INTERFACE_END(interp)

GPU_SHADER_CREATE_INFO(eevee_geom_mesh)
ADDITIONAL_INFO(eevee_shared)
DEFINE("MAT_GEOM_MESH")
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, VEC3, nor)
VERTEX_SOURCE("eevee_geom_mesh_vert.glsl")
VERTEX_OUT(eevee_surf_iface)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_object_infos_new)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_point_cloud_iface, point_cloud_interp)
SMOOTH(FLOAT, radius)
SMOOTH(VEC3, position)
GPU_SHADER_NAMED_INTERFACE_END(point_cloud_interp)
GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_point_cloud_flat_iface, point_cloud_interp_flat)
FLAT(INT, id)
GPU_SHADER_NAMED_INTERFACE_END(point_cloud_interp_flat)

GPU_SHADER_CREATE_INFO(eevee_geom_point_cloud)
ADDITIONAL_INFO(eevee_shared)
DEFINE("MAT_GEOM_POINT_CLOUD")
VERTEX_SOURCE("eevee_geom_point_cloud_vert.glsl")
VERTEX_OUT(eevee_surf_iface)
VERTEX_OUT(eevee_surf_point_cloud_iface)
VERTEX_OUT(eevee_surf_point_cloud_flat_iface)
ADDITIONAL_INFO(draw_pointcloud_new)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_object_infos_new)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_geom_volume)
ADDITIONAL_INFO(eevee_shared)
DEFINE("MAT_GEOM_VOLUME")
VERTEX_IN(0, VEC3, pos)
VERTEX_OUT(eevee_surf_iface)
VERTEX_SOURCE("eevee_geom_volume_vert.glsl")
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_object_infos_new)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_volume_infos)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_geom_gpencil)
ADDITIONAL_INFO(eevee_shared)
DEFINE("MAT_GEOM_GPENCIL")
VERTEX_SOURCE("eevee_geom_gpencil_vert.glsl")
VERTEX_OUT(eevee_surf_iface)
ADDITIONAL_INFO(draw_gpencil_new)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_object_infos_new)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_resource_id_new)
GPU_SHADER_CREATE_END()

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_curve_iface, curve_interp)
SMOOTH(VEC2, barycentric_coords)
SMOOTH(VEC3, tangent)
SMOOTH(VEC3, binormal)
SMOOTH(FLOAT, time)
SMOOTH(FLOAT, time_width)
SMOOTH(FLOAT, thickness)
GPU_SHADER_NAMED_INTERFACE_END(curve_interp)
GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_curve_flat_iface, curve_interp_flat)
FLAT(INT, strand_id)
GPU_SHADER_NAMED_INTERFACE_END(curve_interp_flat)

GPU_SHADER_CREATE_INFO(eevee_geom_curves)
ADDITIONAL_INFO(eevee_shared)
DEFINE("MAT_GEOM_CURVES")
VERTEX_SOURCE("eevee_geom_curves_vert.glsl")
VERTEX_OUT(eevee_surf_iface)
VERTEX_OUT(eevee_surf_curve_iface)
VERTEX_OUT(eevee_surf_curve_flat_iface)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_object_infos_new)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_hair_new)
ADDITIONAL_INFO(draw_curves_infos)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_geom_world)
ADDITIONAL_INFO(eevee_shared)
DEFINE("MAT_GEOM_WORLD")
BUILTINS(BuiltinBits::VERTEX_ID)
VERTEX_SOURCE("eevee_geom_world_vert.glsl")
VERTEX_OUT(eevee_surf_iface)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_object_infos_new) /* Unused, but allow debug compilation. */
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface
 * \{ */

#define image_out(slot, qualifier, format, name) \
  image(slot, format, qualifier, ImageType::FLOAT_2D, name, Frequency::PASS)
#define image_array_out(slot, qualifier, format, name) \
  image(slot, format, qualifier, ImageType::FLOAT_2D_ARRAY, name, Frequency::PASS)

GPU_SHADER_CREATE_INFO(eevee_render_pass_out)
DEFINE("MAT_RENDER_PASS_SUPPORT")
ADDITIONAL_INFO(eevee_global_ubo)
IMAGE_FREQ(RBUFS_COLOR_SLOT, GPU_RGBA16F, WRITE, FLOAT_2D_ARRAY, rp_color_img, PASS)
IMAGE_FREQ(RBUFS_VALUE_SLOT, GPU_R16F, WRITE, FLOAT_2D_ARRAY, rp_value_img, PASS)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_cryptomatte_out)
STORAGE_BUF(CRYPTOMATTE_BUF_SLOT, READ, vec2, cryptomatte_object_buf[])
IMAGE_FREQ(RBUFS_CRYPTOMATTE_SLOT, GPU_RGBA32F, WRITE, FLOAT_2D, rp_cryptomatte_img, PASS)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_deferred_base)
DEFINE("MAT_DEFERRED")
DEFINE("GBUFFER_WRITE")
/* NOTE: This removes the possibility of using gl_FragDepth. */
EARLY_FRAGMENT_TEST(true)
/* Direct output. (Emissive, Holdout) */
FRAGMENT_OUT(0, VEC4, out_radiance)
FRAGMENT_OUT_ROG(1, UINT, out_gbuf_header, DEFERRED_GBUFFER_ROG_ID)
FRAGMENT_OUT(2, VEC2, out_gbuf_normal)
FRAGMENT_OUT(3, VEC4, out_gbuf_closure1)
FRAGMENT_OUT(4, VEC4, out_gbuf_closure2)
/* Everything is stored inside a two layered target, one for each format. This is to fit the
 * limitation of the number of images we can bind on a single shader. */
IMAGE_FREQ(GBUF_CLOSURE_SLOT, GPU_RGB10_A2, WRITE, FLOAT_2D_ARRAY, out_gbuf_closure_img, PASS)
IMAGE_FREQ(GBUF_NORMAL_SLOT, GPU_RG16, WRITE, FLOAT_2D_ARRAY, out_gbuf_normal_img, PASS)
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
FRAGMENT_OUT_DUAL(0, VEC4, out_radiance, SRC_0)
FRAGMENT_OUT_DUAL(0, VEC4, out_transmittance, SRC_1)
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
STORAGE_BUF(SURFEL_BUF_SLOT, WRITE, Surfel, surfel_buf[])
STORAGE_BUF(CAPTURE_BUF_SLOT, READ_WRITE, CaptureInfoData, capture_info_buf)
PUSH_CONSTANT(BOOL, is_double_sided)
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
PUSH_CONSTANT(FLOAT, world_opacity_fade)
PUSH_CONSTANT(FLOAT, world_background_blur)
PUSH_CONSTANT(IVEC4, world_coord_packed)
EARLY_FRAGMENT_TEST(true)
FRAGMENT_OUT(0, VEC4, out_background)
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
FRAGMENT_OUT(0, VEC4, out_background)
FRAGMENT_SOURCE("eevee_renderpass_clear_frag.glsl")
ADDITIONAL_INFO(draw_fullscreen)
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_render_pass_out)
ADDITIONAL_INFO(eevee_cryptomatte_out)
ADDITIONAL_INFO(eevee_shared)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_shadow_atomic_iface, shadow_iface)
FLAT(INT, shadow_view_id)
GPU_SHADER_NAMED_INTERFACE_END(shadow_iface)

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_shadow_clipping_iface, shadow_clip)
SMOOTH(VEC3, position)
SMOOTH(VEC3, vector)
GPU_SHADER_NAMED_INTERFACE_END(shadow_clip)

GPU_SHADER_CREATE_INFO(eevee_surf_shadow)
DEFINE_VALUE("DRW_VIEW_LEN", STRINGIFY(SHADOW_VIEW_MAX))
DEFINE("MAT_SHADOW")
BUILTINS(BuiltinBits::VIEWPORT_INDEX)
VERTEX_OUT(eevee_surf_shadow_clipping_iface)
STORAGE_BUF(SHADOW_RENDER_VIEW_BUF_SLOT, READ, ShadowRenderView, render_view_buf[SHADOW_VIEW_MAX])
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
STORAGE_BUF(SHADOW_RENDER_MAP_BUF_SLOT, READ, uint, render_map_buf[SHADOW_RENDER_MAP_SIZE])
IMAGE(SHADOW_ATLAS_IMG_SLOT, GPU_R32UI, READ_WRITE, UINT_2D_ARRAY_ATOMIC, shadow_atlas_img)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_shadow_tbdr)
ADDITIONAL_INFO(eevee_surf_shadow)
DEFINE("SHADOW_UPDATE_TBDR")
BUILTINS(BuiltinBits::LAYER)
/* Use greater depth write to avoid loosing the early Z depth test but ensure correct fragment
 * ordering after slope bias. */
DEPTH_WRITE(DepthWrite::GREATER)
/* F32 color attachment for on-tile depth accumulation without atomics. */
FRAGMENT_OUT_ROG(0, FLOAT, out_depth, SHADOW_ROG_ID)
GPU_SHADER_CREATE_END()

#undef image_out
#undef image_array_out

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_surf_volume)
DEFINE("MAT_VOLUME")
/* Only the front fragments have to be invoked. */
EARLY_FRAGMENT_TEST(true)
IMAGE(
    VOLUME_PROP_SCATTERING_IMG_SLOT, GPU_R11F_G11F_B10F, READ_WRITE, FLOAT_3D, out_scattering_img)
IMAGE(
    VOLUME_PROP_EXTINCTION_IMG_SLOT, GPU_R11F_G11F_B10F, READ_WRITE, FLOAT_3D, out_extinction_img)
IMAGE(VOLUME_PROP_EMISSION_IMG_SLOT, GPU_R11F_G11F_B10F, READ_WRITE, FLOAT_3D, out_emissive_img)
IMAGE(VOLUME_PROP_PHASE_IMG_SLOT, GPU_R16F, READ_WRITE, FLOAT_3D, out_phase_img)
IMAGE(VOLUME_PROP_PHASE_WEIGHT_IMG_SLOT, GPU_R16F, READ_WRITE, FLOAT_3D, out_phase_weight_img)
IMAGE(VOLUME_OCCUPANCY_SLOT, GPU_R32UI, READ, UINT_3D_ATOMIC, occupancy_img)
FRAGMENT_SOURCE("eevee_surf_volume_frag.glsl")
ADDITIONAL_INFO(draw_modelmat_new_common)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_sampling_data)
ADDITIONAL_INFO(eevee_utility_texture)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_occupancy)
DEFINE("MAT_OCCUPANCY")
/* All fragments need to be invoked even if we write to the depth buffer. */
EARLY_FRAGMENT_TEST(false)
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
PUSH_CONSTANT(BOOL, use_fast_method)
IMAGE(VOLUME_HIT_DEPTH_SLOT, GPU_R32F, WRITE, FLOAT_3D, hit_depth_img)
IMAGE(VOLUME_HIT_COUNT_SLOT, GPU_R32UI, READ_WRITE, UINT_2D_ATOMIC, hit_count_img)
IMAGE(VOLUME_OCCUPANCY_SLOT, GPU_R32UI, READ_WRITE, UINT_3D_ATOMIC, occupancy_img)
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

/* Stub functions defined by the material evaluation. */
GPU_SHADER_CREATE_INFO(eevee_material_stub)
DEFINE("EEVEE_MATERIAL_STUBS")
/* Dummy uniform buffer to detect overlap with material node-tree. */
UNIFORM_BUF(0, int, node_tree)
GPU_SHADER_CREATE_END()

#  define EEVEE_MAT_GEOM_VARIATIONS(prefix, ...) \
    CREATE_INFO_VARIANT(prefix##_world, eevee_geom_world, __VA_ARGS__) \
    /* Turned off until dependency on common_view/math_lib are sorted out. */ \
    /* CREATE_INFO_VARIANT(prefix##_gpencil, eevee_geom_gpencil, __VA_ARGS__) */ \
    CREATE_INFO_VARIANT(prefix##_curves, eevee_geom_curves, __VA_ARGS__) \
    CREATE_INFO_VARIANT(prefix##_mesh, eevee_geom_mesh, __VA_ARGS__) \
    CREATE_INFO_VARIANT(prefix##_point_cloud, eevee_geom_point_cloud, __VA_ARGS__) \
    CREATE_INFO_VARIANT(prefix##_volume, eevee_geom_volume, __VA_ARGS__)

#  define EEVEE_MAT_PIPE_VARIATIONS(name, ...) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_world, eevee_surf_world, __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_depth, eevee_surf_depth, __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_deferred, eevee_surf_deferred, __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_forward, eevee_surf_forward, __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_capture, eevee_surf_capture, __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_volume, eevee_surf_volume, __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_occupancy, eevee_surf_occupancy, __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_shadow_atomic, eevee_surf_shadow_atomic, __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_shadow_tbdr, eevee_surf_shadow_tbdr, __VA_ARGS__)

EEVEE_MAT_PIPE_VARIATIONS(eevee_surface, eevee_material_stub)

#endif

/** \} */
