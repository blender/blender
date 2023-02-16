/* SPDX-License-Identifier: GPL-2.0-or-later */

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
    .storage_buf(6, Qualifier::READ, "SamplingData", "sampling_buf");

GPU_SHADER_CREATE_INFO(eevee_utility_texture)
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

GPU_SHADER_CREATE_INFO(eevee_geom_gpencil)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_GPENCIL")
    .vertex_source("eevee_geom_gpencil_vert.glsl")
    .additional_info("draw_gpencil", "draw_resource_id_varying", "draw_resource_id_new");

GPU_SHADER_CREATE_INFO(eevee_geom_curves)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_CURVES")
    .vertex_source("eevee_geom_curves_vert.glsl")
    .additional_info("draw_hair",
                     "draw_curves_infos",
                     "draw_resource_id_varying",
                     "draw_resource_id_new");

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

GPU_SHADER_CREATE_INFO(eevee_aov_out)
    .define("MAT_AOV_SUPPORT")
    .image_array_out(RBUFS_AOV_COLOR_SLOT, Qualifier::WRITE, GPU_RGBA16F, "aov_color_img")
    .image_array_out(RBUFS_AOV_VALUE_SLOT, Qualifier::WRITE, GPU_R16F, "aov_value_img")
    .storage_buf(RBUFS_AOV_BUF_SLOT, Qualifier::READ, "AOVsInfoData", "aov_buf");

GPU_SHADER_CREATE_INFO(eevee_render_pass_out)
    .define("MAT_RENDER_PASS_SUPPORT")
    .image_out(RBUFS_NORMAL_SLOT, Qualifier::READ_WRITE, GPU_RGBA16F, "rp_normal_img")
    .image_array_out(RBUFS_LIGHT_SLOT, Qualifier::READ_WRITE, GPU_RGBA16F, "rp_light_img")
    .image_out(RBUFS_DIFF_COLOR_SLOT, Qualifier::READ_WRITE, GPU_RGBA16F, "rp_diffuse_color_img")
    .image_out(RBUFS_SPEC_COLOR_SLOT, Qualifier::READ_WRITE, GPU_RGBA16F, "rp_specular_color_img")
    .image_out(RBUFS_EMISSION_SLOT, Qualifier::READ_WRITE, GPU_RGBA16F, "rp_emission_img");

GPU_SHADER_CREATE_INFO(eevee_cryptomatte_out)
    .storage_buf(CRYPTOMATTE_BUF_SLOT, Qualifier::READ, "vec2", "cryptomatte_object_buf[]")
    .image_out(RBUFS_CRYPTOMATTE_SLOT, Qualifier::WRITE, GPU_RGBA32F, "rp_cryptomatte_img");

GPU_SHADER_CREATE_INFO(eevee_surf_deferred)
    .vertex_out(eevee_surf_iface)
    /* NOTE: This removes the possibility of using gl_FragDepth. */
    // .early_fragment_test(true)
    /* Direct output. */
    .fragment_out(0, Type::VEC4, "out_radiance", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "out_transmittance", DualBlend::SRC_1)
    /* Gbuffer. */
    // .image_out(0, Qualifier::WRITE, GPU_R11F_G11F_B10F, "gbuff_transmit_color")
    // .image_out(1, Qualifier::WRITE, GPU_R11F_G11F_B10F, "gbuff_transmit_data")
    // .image_out(2, Qualifier::WRITE, GPU_RGBA16F, "gbuff_transmit_normal")
    // .image_out(3, Qualifier::WRITE, GPU_R11F_G11F_B10F, "gbuff_reflection_color")
    // .image_out(4, Qualifier::WRITE, GPU_RGBA16F, "gbuff_reflection_normal")
    // .image_out(5, Qualifier::WRITE, GPU_R11F_G11F_B10F, "gbuff_emission")
    /* Render-passes. */
    // .image_out(6, Qualifier::READ_WRITE, GPU_RGBA16F, "rpass_volume_light")
    .fragment_source("eevee_surf_deferred_frag.glsl")
    .additional_info("eevee_camera",
                     "eevee_utility_texture",
                     "eevee_sampling_data",
                     "eevee_aov_out");

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
                     "eevee_shadow_data"
                     /* Optionally added depending on the material. */
                     // "eevee_cryptomatte_out",
                     // "eevee_raytrace_data",
                     // "eevee_transmittance_data",
                     // "eevee_aov_out",
                     // "eevee_render_pass_out",
    );

GPU_SHADER_CREATE_INFO(eevee_surf_depth)
    .vertex_out(eevee_surf_iface)
    .fragment_source("eevee_surf_depth_frag.glsl")
    .additional_info("eevee_sampling_data", "eevee_camera", "eevee_utility_texture");

GPU_SHADER_CREATE_INFO(eevee_surf_world)
    .vertex_out(eevee_surf_iface)
    .push_constant(Type::FLOAT, "world_opacity_fade")
    .fragment_out(0, Type::VEC4, "out_background")
    .fragment_source("eevee_surf_world_frag.glsl")
    .additional_info("eevee_aov_out",
                     "eevee_cryptomatte_out",
                     "eevee_render_pass_out",
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
GPU_SHADER_CREATE_INFO(eevee_material_stub).define("EEVEE_MATERIAL_STUBS");

#  define EEVEE_MAT_FINAL_VARIATION(name, ...) \
    GPU_SHADER_CREATE_INFO(name).additional_info(__VA_ARGS__).do_static_compilation(true);

#  define EEVEE_MAT_GEOM_VARIATIONS(prefix, ...) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_world, "eevee_geom_world", __VA_ARGS__) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_gpencil, "eevee_geom_gpencil", __VA_ARGS__) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_curves, "eevee_geom_curves", __VA_ARGS__) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_mesh, "eevee_geom_mesh", __VA_ARGS__)

#  define EEVEE_MAT_PIPE_VARIATIONS(name, ...) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_world, "eevee_surf_world", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_depth, "eevee_surf_depth", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_deferred, "eevee_surf_deferred", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_forward, "eevee_surf_forward", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_shadow, "eevee_surf_shadow", __VA_ARGS__)

EEVEE_MAT_PIPE_VARIATIONS(eevee_surface, "eevee_material_stub")

#endif

/** \} */
