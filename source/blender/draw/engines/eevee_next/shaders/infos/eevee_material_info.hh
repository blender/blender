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
    .typedef_source("eevee_defines.hh")
    .typedef_source("eevee_shader_shared.hh");

GPU_SHADER_CREATE_INFO(eevee_global_ubo)
    .uniform_buf(UNIFORM_BUF_SLOT, "UniformData", "uniform_buf");

GPU_SHADER_CREATE_INFO(eevee_sampling_data)
    .define("EEVEE_SAMPLING_DATA")
    .additional_info("eevee_shared")
    .storage_buf(SAMPLING_BUF_SLOT, Qualifier::READ, "SamplingData", "sampling_buf");

GPU_SHADER_CREATE_INFO(eevee_utility_texture)
    .define("EEVEE_UTILITY_TX")
    .sampler(RBUFS_UTILITY_TEX_SLOT, ImageType::FLOAT_2D_ARRAY, "utility_tx");

GPU_SHADER_INTERFACE_INFO(eevee_clip_plane_iface, "clip_interp")
    .smooth(Type::FLOAT, "clip_distance");

GPU_SHADER_CREATE_INFO(eevee_clip_plane)
    .vertex_out(eevee_clip_plane_iface)
    .uniform_buf(CLIP_PLANE_BUF, "ClipPlaneData", "clip_plane")
    .define("MAT_CLIP_PLANE");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface Mesh Type
 * \{ */

/* Common interface */
GPU_SHADER_INTERFACE_INFO(eevee_surf_iface, "interp")
    /* World Position. */
    .smooth(Type::VEC3, "P")
    /* World Normal. */
    .smooth(Type::VEC3, "N");

GPU_SHADER_CREATE_INFO(eevee_geom_mesh)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_MESH")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "nor")
    .vertex_source("eevee_geom_mesh_vert.glsl")
    .vertex_out(eevee_surf_iface)
    .additional_info("draw_modelmat_new", "draw_resource_id_varying", "draw_view");

GPU_SHADER_INTERFACE_INFO(eevee_surf_point_cloud_iface, "point_cloud_interp")
    .smooth(Type::FLOAT, "radius")
    .smooth(Type::VEC3, "position");
GPU_SHADER_INTERFACE_INFO(eevee_surf_point_cloud_flat_iface, "point_cloud_interp_flat")
    .flat(Type::INT, "id");

GPU_SHADER_CREATE_INFO(eevee_geom_point_cloud)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_POINT_CLOUD")
    .vertex_source("eevee_geom_point_cloud_vert.glsl")
    .vertex_out(eevee_surf_iface)
    .vertex_out(eevee_surf_point_cloud_iface)
    .vertex_out(eevee_surf_point_cloud_flat_iface)
    .additional_info("draw_pointcloud_new",
                     "draw_modelmat_new",
                     "draw_resource_id_varying",
                     "draw_view");

GPU_SHADER_CREATE_INFO(eevee_geom_volume)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_VOLUME")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_out(eevee_surf_iface)
    .vertex_source("eevee_geom_volume_vert.glsl")
    .additional_info("draw_modelmat_new",
                     "draw_object_infos_new",
                     "draw_resource_id_varying",
                     "draw_volume_infos",
                     "draw_view");

GPU_SHADER_CREATE_INFO(eevee_geom_gpencil)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_GPENCIL")
    .vertex_source("eevee_geom_gpencil_vert.glsl")
    .vertex_out(eevee_surf_iface)
    .additional_info("draw_gpencil_new", "draw_resource_id_varying", "draw_resource_id_new");

GPU_SHADER_INTERFACE_INFO(eevee_surf_curve_iface, "curve_interp")
    .smooth(Type::VEC2, "barycentric_coords")
    .smooth(Type::VEC3, "tangent")
    .smooth(Type::VEC3, "binormal")
    .smooth(Type::FLOAT, "time")
    .smooth(Type::FLOAT, "time_width")
    .smooth(Type::FLOAT, "thickness");
GPU_SHADER_INTERFACE_INFO(eevee_surf_curve_flat_iface, "curve_interp_flat")
    .flat(Type::INT, "strand_id");

GPU_SHADER_CREATE_INFO(eevee_geom_curves)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_CURVES")
    .vertex_source("eevee_geom_curves_vert.glsl")
    .vertex_out(eevee_surf_iface)
    .vertex_out(eevee_surf_curve_iface)
    .vertex_out(eevee_surf_curve_flat_iface)
    .additional_info("draw_modelmat_new",
                     "draw_resource_id_varying",
                     "draw_view",
                     "draw_hair_new",
                     "draw_curves_infos");

GPU_SHADER_CREATE_INFO(eevee_geom_world)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_WORLD")
    .builtins(BuiltinBits::VERTEX_ID)
    .vertex_source("eevee_geom_world_vert.glsl")
    .vertex_out(eevee_surf_iface)
    .additional_info("draw_modelmat_new", "draw_resource_id_varying", "draw_view");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface
 * \{ */

#define image_out(slot, qualifier, format, name) \
  image(slot, format, qualifier, ImageType::FLOAT_2D, name, Frequency::PASS)
#define image_array_out(slot, qualifier, format, name) \
  image(slot, format, qualifier, ImageType::FLOAT_2D_ARRAY, name, Frequency::PASS)

GPU_SHADER_CREATE_INFO(eevee_render_pass_out)
    .define("MAT_RENDER_PASS_SUPPORT")
    .additional_info("eevee_global_ubo")
    .image_array_out(RBUFS_COLOR_SLOT, Qualifier::WRITE, GPU_RGBA16F, "rp_color_img")
    .image_array_out(RBUFS_VALUE_SLOT, Qualifier::WRITE, GPU_R16F, "rp_value_img");

GPU_SHADER_CREATE_INFO(eevee_cryptomatte_out)
    .storage_buf(CRYPTOMATTE_BUF_SLOT, Qualifier::READ, "vec2", "cryptomatte_object_buf[]")
    .image_out(RBUFS_CRYPTOMATTE_SLOT, Qualifier::WRITE, GPU_RGBA32F, "rp_cryptomatte_img");

GPU_SHADER_CREATE_INFO(eevee_surf_deferred_base)
    .define("MAT_DEFERRED")
    .define("GBUFFER_WRITE")
    /* NOTE: This removes the possibility of using gl_FragDepth. */
    .early_fragment_test(true)
    /* Direct output. (Emissive, Holdout) */
    .fragment_out(0, Type::VEC4, "out_radiance")
    .fragment_out(1, Type::UINT, "out_gbuf_header", DualBlend::NONE, DEFERRED_GBUFFER_ROG_ID)
    .fragment_out(2, Type::VEC2, "out_gbuf_normal")
    .fragment_out(3, Type::VEC4, "out_gbuf_closure1")
    .fragment_out(4, Type::VEC4, "out_gbuf_closure2")
    /* Everything is stored inside a two layered target, one for each format. This is to fit the
     * limitation of the number of images we can bind on a single shader. */
    .image_array_out(GBUF_CLOSURE_SLOT, Qualifier::WRITE, GPU_RGB10_A2, "out_gbuf_closure_img")
    .image_array_out(GBUF_NORMAL_SLOT, Qualifier::WRITE, GPU_RG16, "out_gbuf_normal_img")
    .additional_info("eevee_global_ubo",
                     "eevee_utility_texture",
                     /* Added at runtime because of test shaders not having `node_tree`. */
                     // "eevee_render_pass_out",
                     // "eevee_cryptomatte_out",
                     "eevee_sampling_data",
                     "eevee_hiz_data");

GPU_SHADER_CREATE_INFO(eevee_surf_deferred)
    .fragment_source("eevee_surf_deferred_frag.glsl")
    .additional_info("eevee_surf_deferred_base");

GPU_SHADER_CREATE_INFO(eevee_surf_deferred_hybrid)
    .fragment_source("eevee_surf_hybrid_frag.glsl")
    .additional_info("eevee_surf_deferred_base",
                     "eevee_light_data",
                     "eevee_lightprobe_data",
                     "eevee_shadow_data");

GPU_SHADER_CREATE_INFO(eevee_surf_forward)
    .define("MAT_FORWARD")
    /* Early fragment test is needed for render passes support for forward surfaces. */
    /* NOTE: This removes the possibility of using gl_FragDepth. */
    .early_fragment_test(true)
    .fragment_out(0, Type::VEC4, "out_radiance", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "out_transmittance", DualBlend::SRC_1)
    .fragment_source("eevee_surf_forward_frag.glsl")
    .additional_info("eevee_global_ubo",
                     "eevee_light_data",
                     "eevee_lightprobe_data",
                     "eevee_utility_texture",
                     "eevee_sampling_data",
                     "eevee_shadow_data",
                     "eevee_hiz_data",
                     "eevee_volume_lib"
                     /* Optionally added depending on the material. */
                     // "eevee_render_pass_out",
                     // "eevee_cryptomatte_out",
    );

GPU_SHADER_CREATE_INFO(eevee_surf_capture)
    .define("MAT_CAPTURE")
    .storage_buf(SURFEL_BUF_SLOT, Qualifier::WRITE, "Surfel", "surfel_buf[]")
    .storage_buf(CAPTURE_BUF_SLOT, Qualifier::READ_WRITE, "CaptureInfoData", "capture_info_buf")
    .push_constant(Type::BOOL, "is_double_sided")
    .fragment_source("eevee_surf_capture_frag.glsl")
    .additional_info("eevee_global_ubo", "eevee_utility_texture");

GPU_SHADER_CREATE_INFO(eevee_surf_depth)
    .define("MAT_DEPTH")
    .fragment_source("eevee_surf_depth_frag.glsl")
    .additional_info("eevee_global_ubo", "eevee_sampling_data", "eevee_utility_texture");

GPU_SHADER_CREATE_INFO(eevee_surf_world)
    .push_constant(Type::FLOAT, "world_opacity_fade")
    .push_constant(Type::FLOAT, "world_background_blur")
    .push_constant(Type::IVEC4, "world_coord_packed")
    .fragment_out(0, Type::VEC4, "out_background")
    .fragment_source("eevee_surf_world_frag.glsl")
    .additional_info("eevee_global_ubo",
                     "eevee_reflection_probe_data",
                     "eevee_volume_probe_data",
                     "eevee_sampling_data",
                     /* Optionally added depending on the material. */
                     //  "eevee_render_pass_out",
                     //  "eevee_cryptomatte_out",
                     "eevee_utility_texture");

GPU_SHADER_INTERFACE_INFO(eevee_surf_shadow_atomic_iface, "shadow_iface")
    .flat(Type::INT, "shadow_view_id");

GPU_SHADER_INTERFACE_INFO(eevee_surf_shadow_clipping_iface, "shadow_clip")
    .smooth(Type::VEC3, "vector");

GPU_SHADER_CREATE_INFO(eevee_surf_shadow)
    .define("DRW_VIEW_LEN", STRINGIFY(SHADOW_VIEW_MAX))
    .define("MAT_SHADOW")
    .builtins(BuiltinBits::VIEWPORT_INDEX)
    .vertex_out(eevee_surf_shadow_clipping_iface)
    .storage_buf(SHADOW_RENDER_VIEW_BUF_SLOT,
                 Qualifier::READ,
                 "ShadowRenderView",
                 "render_view_buf[SHADOW_VIEW_MAX]")
    .fragment_source("eevee_surf_shadow_frag.glsl")
    .additional_info("eevee_global_ubo", "eevee_utility_texture", "eevee_sampling_data");

GPU_SHADER_CREATE_INFO(eevee_surf_shadow_atomic)
    .additional_info("eevee_surf_shadow")
    .define("SHADOW_UPDATE_ATOMIC_RASTER")
    .builtins(BuiltinBits::TEXTURE_ATOMIC)
    /* Early fragment test for speeding up platforms that requires a depth buffer. */
    /* NOTE: This removes the possibility of using gl_FragDepth. */
    .early_fragment_test(true)
    .vertex_out(eevee_surf_shadow_atomic_iface)
    .storage_buf(SHADOW_RENDER_MAP_BUF_SLOT,
                 Qualifier::READ,
                 "uint",
                 "render_map_buf[SHADOW_RENDER_MAP_SIZE]")
    .image(SHADOW_ATLAS_IMG_SLOT,
           GPU_R32UI,
           Qualifier::READ_WRITE,
           ImageType::UINT_2D_ARRAY_ATOMIC,
           "shadow_atlas_img");

GPU_SHADER_CREATE_INFO(eevee_surf_shadow_tbdr)
    .additional_info("eevee_surf_shadow")
    .define("SHADOW_UPDATE_TBDR")
    .builtins(BuiltinBits::LAYER)
    /* F32 color attachment for on-tile depth accumulation without atomics. */
    .fragment_out(0, Type::FLOAT, "out_depth", DualBlend::NONE, SHADOW_ROG_ID);

#undef image_out
#undef image_array_out

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_surf_volume)
    .define("MAT_VOLUME")
    /* Only the front fragments have to be invoked. */
    .early_fragment_test(true)
    .image(VOLUME_PROP_SCATTERING_IMG_SLOT,
           GPU_R11F_G11F_B10F,
           Qualifier::READ_WRITE,
           ImageType::FLOAT_3D,
           "out_scattering_img")
    .image(VOLUME_PROP_EXTINCTION_IMG_SLOT,
           GPU_R11F_G11F_B10F,
           Qualifier::READ_WRITE,
           ImageType::FLOAT_3D,
           "out_extinction_img")
    .image(VOLUME_PROP_EMISSION_IMG_SLOT,
           GPU_R11F_G11F_B10F,
           Qualifier::READ_WRITE,
           ImageType::FLOAT_3D,
           "out_emissive_img")
    .image(VOLUME_PROP_PHASE_IMG_SLOT,
           GPU_RG16F,
           Qualifier::READ_WRITE,
           ImageType::FLOAT_3D,
           "out_phase_img")
    .image(VOLUME_OCCUPANCY_SLOT,
           GPU_R32UI,
           Qualifier::READ,
           ImageType::UINT_3D_ATOMIC,
           "occupancy_img")
    .fragment_source("eevee_surf_volume_frag.glsl")
    .additional_info("draw_modelmat_new_common",
                     "draw_view",
                     "eevee_shared",
                     "eevee_global_ubo",
                     "eevee_sampling_data",
                     "eevee_utility_texture");

GPU_SHADER_CREATE_INFO(eevee_surf_occupancy)
    .define("MAT_OCCUPANCY")
    /* All fragments need to be invoked even if we write to the depth buffer. */
    .early_fragment_test(false)
    .builtins(BuiltinBits::TEXTURE_ATOMIC)
    .push_constant(Type::BOOL, "use_fast_method")
    .image(VOLUME_HIT_DEPTH_SLOT, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_3D, "hit_depth_img")
    .image(VOLUME_HIT_COUNT_SLOT,
           GPU_R32UI,
           Qualifier::READ_WRITE,
           ImageType::UINT_2D_ATOMIC,
           "hit_count_img")
    .image(VOLUME_OCCUPANCY_SLOT,
           GPU_R32UI,
           Qualifier::READ_WRITE,
           ImageType::UINT_3D_ATOMIC,
           "occupancy_img")
    .fragment_source("eevee_surf_occupancy_frag.glsl")
    .additional_info("eevee_global_ubo", "eevee_sampling_data");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test shaders
 *
 * Variations that are only there to test shaders at compile time.
 * \{ */

#ifndef NDEBUG

/* Stub functions defined by the material evaluation. */
GPU_SHADER_CREATE_INFO(eevee_material_stub)
    .define("EEVEE_MATERIAL_STUBS")
    /* Dummy uniform buffer to detect overlap with material node-tree. */
    .uniform_buf(0, "int", "node_tree");

#  define EEVEE_MAT_FINAL_VARIATION(name, ...) \
    GPU_SHADER_CREATE_INFO(name).additional_info(__VA_ARGS__).do_static_compilation(true);

#  define EEVEE_MAT_GEOM_VARIATIONS(prefix, ...) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_world, "eevee_geom_world", __VA_ARGS__) \
    /* Turned off until dependency on common_view/math_lib are sorted out. */ \
    /* EEVEE_MAT_FINAL_VARIATION(prefix##_gpencil, "eevee_geom_gpencil", __VA_ARGS__) */ \
    EEVEE_MAT_FINAL_VARIATION(prefix##_curves, "eevee_geom_curves", __VA_ARGS__) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_mesh, "eevee_geom_mesh", __VA_ARGS__) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_point_cloud, "eevee_geom_point_cloud", __VA_ARGS__) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_volume, "eevee_geom_volume", __VA_ARGS__)

#  define EEVEE_MAT_PIPE_VARIATIONS(name, ...) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_world, "eevee_surf_world", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_depth, "eevee_surf_depth", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_deferred, "eevee_surf_deferred", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_forward, "eevee_surf_forward", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_capture, "eevee_surf_capture", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_volume, "eevee_surf_volume", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_occupancy, "eevee_surf_occupancy", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_shadow_atomic, "eevee_surf_shadow_atomic", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_shadow_tbdr, "eevee_surf_shadow_tbdr", __VA_ARGS__)

EEVEE_MAT_PIPE_VARIATIONS(eevee_surface, "eevee_material_stub")

#endif

/** \} */
