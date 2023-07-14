/* SPDX-FileCopyrightText: 2023 Blender Foundation
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

GPU_SHADER_CREATE_INFO(eevee_sampling_data)
    .define("EEVEE_SAMPLING_DATA")
    .additional_info("eevee_shared")
    .storage_buf(SAMPLING_BUF_SLOT, Qualifier::READ, "SamplingData", "sampling_buf");

GPU_SHADER_CREATE_INFO(eevee_utility_texture)
    .define("EEVEE_UTILITY_TX")
    .sampler(RBUFS_UTILITY_TEX_SLOT, ImageType::FLOAT_2D_ARRAY, "utility_tx");

GPU_SHADER_CREATE_INFO(eevee_camera).uniform_buf(CAMERA_BUF_SLOT, "CameraData", "camera_buf");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface Mesh Type
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_geom_mesh)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_MESH")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "nor")
    .vertex_source("eevee_geom_mesh_vert.glsl")
    .additional_info("draw_modelmat_new", "draw_resource_id_varying", "draw_view");

GPU_SHADER_INTERFACE_INFO(eevee_surf_point_cloud_iface, "point_cloud_interp")
    .smooth(Type::FLOAT, "radius")
    .smooth(Type::VEC3, "position")
    .flat(Type::INT, "id");

GPU_SHADER_CREATE_INFO(eevee_geom_point_cloud)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_POINT_CLOUD")
    .vertex_source("eevee_geom_point_cloud_vert.glsl")
    .vertex_out(eevee_surf_point_cloud_iface)
    /* TODO(Miguel Pozo): Remove once we get rid of old EEVEE. */
    .define("pointRadius", "point_cloud_interp.radius")
    .define("pointPosition", "point_cloud_interp.position")
    .define("pointID", "point_cloud_interp.id")
    .additional_info("draw_pointcloud_new",
                     "draw_modelmat_new",
                     "draw_resource_id_varying",
                     "draw_view");

GPU_SHADER_CREATE_INFO(eevee_geom_gpencil)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_GPENCIL")
    .vertex_source("eevee_geom_gpencil_vert.glsl")
    .additional_info("draw_gpencil_new", "draw_resource_id_varying", "draw_resource_id_new");

GPU_SHADER_CREATE_INFO(eevee_geom_curves)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_CURVES")
    .vertex_source("eevee_geom_curves_vert.glsl")
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
    .additional_info("draw_modelmat_new", "draw_resource_id_varying", "draw_view");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface
 * \{ */

GPU_SHADER_INTERFACE_INFO(eevee_surf_iface, "interp")
    .smooth(Type::VEC3, "P")
    .smooth(Type::VEC3, "N")
    .smooth(Type::VEC2, "barycentric_coords")
    .smooth(Type::VEC3, "curves_tangent")
    .smooth(Type::VEC3, "curves_binormal")
    .smooth(Type::FLOAT, "curves_time")
    .smooth(Type::FLOAT, "curves_time_width")
    .smooth(Type::FLOAT, "curves_thickness")
    .flat(Type::INT, "curves_strand_id");

#define image_out(slot, qualifier, format, name) \
  image(slot, format, qualifier, ImageType::FLOAT_2D, name, Frequency::PASS)
#define image_array_out(slot, qualifier, format, name) \
  image(slot, format, qualifier, ImageType::FLOAT_2D_ARRAY, name, Frequency::PASS)

GPU_SHADER_CREATE_INFO(eevee_render_pass_out)
    .define("MAT_RENDER_PASS_SUPPORT")
    .image_array_out(RBUFS_COLOR_SLOT, Qualifier::WRITE, GPU_RGBA16F, "rp_color_img")
    .image_array_out(RBUFS_VALUE_SLOT, Qualifier::WRITE, GPU_R16F, "rp_value_img")
    .uniform_buf(RBUFS_BUF_SLOT, "RenderBuffersInfoData", "rp_buf");

GPU_SHADER_CREATE_INFO(eevee_cryptomatte_out)
    .storage_buf(CRYPTOMATTE_BUF_SLOT, Qualifier::READ, "vec2", "cryptomatte_object_buf[]")
    .image_out(RBUFS_CRYPTOMATTE_SLOT, Qualifier::WRITE, GPU_RGBA32F, "rp_cryptomatte_img");

GPU_SHADER_CREATE_INFO(eevee_surf_deferred)
    .vertex_out(eevee_surf_iface)
    /* NOTE: This removes the possibility of using gl_FragDepth. */
    .early_fragment_test(true)
    /* Direct output. (Emissive, Holdout) */
    .fragment_out(0, Type::VEC4, "out_radiance", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "out_transmittance", DualBlend::SRC_1)
    /* Everything is stored inside a two layered target, one for each format. This is to fit the
     * limitation of the number of images we can bind on a single shader. */
    .image_array_out(GBUF_CLOSURE_SLOT, Qualifier::WRITE, GPU_RGBA16, "out_gbuff_closure_img")
    .image_array_out(GBUF_COLOR_SLOT, Qualifier::WRITE, GPU_RGB10_A2, "out_gbuff_color_img")
    .fragment_source("eevee_surf_deferred_frag.glsl")
    .additional_info("eevee_camera",
                     "eevee_utility_texture",
                     "eevee_sampling_data",
                     /* Added at runtime because of test shaders not having `node_tree`. */
                     //  "eevee_render_pass_out",
                     "eevee_cryptomatte_out",
                     "eevee_ambient_occlusion_data");

GPU_SHADER_CREATE_INFO(eevee_surf_forward)
    .vertex_out(eevee_surf_iface)
    /* Early fragment test is needed for render passes support for forward surfaces. */
    /* NOTE: This removes the possibility of using gl_FragDepth. */
    .early_fragment_test(true)
    .fragment_out(0, Type::VEC4, "out_radiance", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "out_transmittance", DualBlend::SRC_1)
    .fragment_source("eevee_surf_forward_frag.glsl")
    .additional_info("eevee_light_data",
                     "eevee_camera",
                     "eevee_utility_texture",
                     "eevee_sampling_data",
                     "eevee_shadow_data",
                     "eevee_ambient_occlusion_data"
                     /* Optionally added depending on the material. */
                     // "eevee_render_pass_out",
                     // "eevee_cryptomatte_out",
                     // "eevee_raytrace_data",
                     // "eevee_transmittance_data",
    );

GPU_SHADER_CREATE_INFO(eevee_surf_capture)
    .vertex_out(eevee_surf_iface)
    .define("MAT_CAPTURE")
    .storage_buf(SURFEL_BUF_SLOT, Qualifier::WRITE, "Surfel", "surfel_buf[]")
    .storage_buf(CAPTURE_BUF_SLOT, Qualifier::READ_WRITE, "CaptureInfoData", "capture_info_buf")
    .fragment_source("eevee_surf_capture_frag.glsl")
    .additional_info("eevee_camera", "eevee_utility_texture");

GPU_SHADER_CREATE_INFO(eevee_surf_depth)
    .define("MAT_DEPTH")
    .vertex_out(eevee_surf_iface)
    .fragment_source("eevee_surf_depth_frag.glsl")
    .additional_info("eevee_sampling_data", "eevee_camera", "eevee_utility_texture");

GPU_SHADER_CREATE_INFO(eevee_surf_world)
    .vertex_out(eevee_surf_iface)
    .push_constant(Type::FLOAT, "world_opacity_fade")
    .fragment_out(0, Type::VEC4, "out_background")
    .fragment_source("eevee_surf_world_frag.glsl")
    .additional_info("eevee_render_pass_out",
                     "eevee_cryptomatte_out",
                     "eevee_camera",
                     "eevee_utility_texture");

GPU_SHADER_INTERFACE_INFO(eevee_shadow_iface, "shadow_interp").flat(Type::UINT, "view_id");

GPU_SHADER_CREATE_INFO(eevee_surf_shadow)
    .define("DRW_VIEW_LEN", "64")
    .define("MAT_SHADOW")
    .vertex_out(eevee_surf_iface)
    .vertex_out(eevee_shadow_iface)
    .sampler(SHADOW_RENDER_MAP_SLOT, ImageType::UINT_2D_ARRAY, "shadow_render_map_tx")
    .image(SHADOW_ATLAS_SLOT,
           GPU_R32UI,
           Qualifier::READ_WRITE,
           ImageType::UINT_2D,
           "shadow_atlas_img")
    .storage_buf(SHADOW_PAGE_INFO_SLOT, Qualifier::READ, "ShadowPagesInfoData", "pages_infos_buf")
    .fragment_source("eevee_surf_shadow_frag.glsl")
    .additional_info("eevee_camera", "eevee_utility_texture", "eevee_sampling_data");

#undef image_out
#undef image_array_out

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume
 * \{ */

#if 0 /* TODO */
GPU_SHADER_INTERFACE_INFO(eevee_volume_iface, "interp")
    .smooth(Type::VEC3, "P_start")
    .smooth(Type::VEC3, "P_end");

GPU_SHADER_CREATE_INFO(eevee_volume_deferred)
    .sampler(0, ImageType::DEPTH_2D, "depth_max_tx")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_out(eevee_volume_iface)
    .fragment_out(0, Type::UVEC4, "out_volume_data")
    .fragment_out(1, Type::VEC4, "out_transparency_data")
    .additional_info("eevee_shared")
    .vertex_source("eevee_volume_vert.glsl")
    .fragment_source("eevee_volume_deferred_frag.glsl")
    .additional_info("draw_fullscreen");
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Test shaders
 *
 * Variations that are only there to test shaders at compile time.
 * \{ */

#ifdef DEBUG

/* Stub functions defined by the material evaluation. */
GPU_SHADER_CREATE_INFO(eevee_material_stub)
    .define("EEVEE_MATERIAL_STUBS")
    /* Dummy uniform buffer to detect overlap with material node-tree. */
    .uniform_buf(0, "int", "node_tree");

#  define EEVEE_MAT_FINAL_VARIATION(name, ...) \
    GPU_SHADER_CREATE_INFO(name).additional_info(__VA_ARGS__).do_static_compilation(true);

#  define EEVEE_MAT_GEOM_VARIATIONS(prefix, ...) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_world, "eevee_geom_world", __VA_ARGS__) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_gpencil, "eevee_geom_gpencil", __VA_ARGS__) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_curves, "eevee_geom_curves", __VA_ARGS__) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_mesh, "eevee_geom_mesh", __VA_ARGS__) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_point_cloud, "eevee_geom_point_cloud", __VA_ARGS__)

#  define EEVEE_MAT_PIPE_VARIATIONS(name, ...) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_world, "eevee_surf_world", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_depth, "eevee_surf_depth", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_deferred, "eevee_surf_deferred", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_forward, "eevee_surf_forward", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_capture, "eevee_surf_capture", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_shadow, "eevee_surf_shadow", __VA_ARGS__)

EEVEE_MAT_PIPE_VARIATIONS(eevee_surface, "eevee_material_stub")

#endif

/** \} */
