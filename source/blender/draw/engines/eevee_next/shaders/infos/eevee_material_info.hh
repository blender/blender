/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

/* TODO(@fclem): This is a bit out of place at the moment. */
GPU_SHADER_CREATE_INFO(eevee_shared)
    .typedef_source("eevee_defines.hh")
    .typedef_source("eevee_shader_shared.hh");

GPU_SHADER_CREATE_INFO(eevee_sampling_data)
    .additional_info("eevee_shared")
    .uniform_buf(14, "SamplingData", "sampling_buf");

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
    .additional_info("draw_mesh", "draw_resource_id_varying", "draw_resource_handle");

GPU_SHADER_CREATE_INFO(eevee_geom_gpencil)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_GPENCIL")
    .vertex_source("eevee_geom_gpencil_vert.glsl")
    .additional_info("draw_gpencil", "draw_resource_id_varying", "draw_resource_handle");

GPU_SHADER_CREATE_INFO(eevee_geom_curves)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_CURVES")
    .vertex_source("eevee_geom_curves_vert.glsl")
    .additional_info("draw_hair",
                     "draw_curves_infos",
                     "draw_resource_id_varying",
                     "draw_resource_handle");

GPU_SHADER_CREATE_INFO(eevee_geom_world)
    .additional_info("eevee_shared")
    .define("MAT_GEOM_WORLD")
    .builtins(BuiltinBits::VERTEX_ID)
    .vertex_source("eevee_geom_world_vert.glsl")
    .additional_info("draw_modelmat", "draw_resource_id_varying", "draw_resource_handle");

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
    /* Renderpasses. */
    // .image_out(6, Qualifier::READ_WRITE, GPU_RGBA16F, "rpass_volume_light")
    /* TODO: AOVs maybe? */
    .fragment_source("eevee_surf_deferred_frag.glsl")
    // .additional_info("eevee_sampling_data", "eevee_utility_texture")
    ;

#undef image_out

GPU_SHADER_CREATE_INFO(eevee_surf_forward)
    .auto_resource_location(true)
    .vertex_out(eevee_surf_iface)
    .fragment_out(0, Type::VEC4, "out_radiance", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "out_transmittance", DualBlend::SRC_1)
    .fragment_source("eevee_surf_forward_frag.glsl")
    // .additional_info("eevee_sampling_data",
    //  "eevee_lightprobe_data",
    /* Optionally added depending on the material. */
    // "eevee_raytrace_data",
    // "eevee_transmittance_data",
    //  "eevee_utility_texture",
    //  "eevee_light_data",
    //  "eevee_shadow_data"
    // )
    ;

GPU_SHADER_CREATE_INFO(eevee_surf_depth)
    .vertex_out(eevee_surf_iface)
    .fragment_source("eevee_surf_depth_frag.glsl")
    // .additional_info("eevee_sampling_data", "eevee_utility_texture")
    ;

GPU_SHADER_CREATE_INFO(eevee_surf_world)
    .vertex_out(eevee_surf_iface)
    .fragment_out(0, Type::VEC4, "out_background")
    .fragment_source("eevee_surf_world_frag.glsl")
    // .additional_info("eevee_utility_texture")
    ;

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
    GPU_SHADER_CREATE_INFO(name) \
        .additional_info(__VA_ARGS__) \
        .auto_resource_location(true) \
        .do_static_compilation(true);

#  define EEVEE_MAT_GEOM_VARIATIONS(prefix, ...) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_world, "eevee_geom_world", __VA_ARGS__) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_gpencil, "eevee_geom_gpencil", __VA_ARGS__) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_curves, "eevee_geom_curves", __VA_ARGS__) \
    EEVEE_MAT_FINAL_VARIATION(prefix##_mesh, "eevee_geom_mesh", __VA_ARGS__)

#  define EEVEE_MAT_PIPE_VARIATIONS(name, ...) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_world, "eevee_surf_world", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_depth, "eevee_surf_depth", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_deferred, "eevee_surf_deferred", __VA_ARGS__) \
    EEVEE_MAT_GEOM_VARIATIONS(name##_forward, "eevee_surf_forward", __VA_ARGS__)

EEVEE_MAT_PIPE_VARIATIONS(eevee_surface, "eevee_material_stub")

#endif

/** \} */
