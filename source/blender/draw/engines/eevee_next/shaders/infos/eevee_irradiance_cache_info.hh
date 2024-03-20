/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Display
 * \{ */

GPU_SHADER_INTERFACE_INFO(eevee_debug_surfel_iface, "")
    .smooth(Type::VEC3, "P")
    .flat(Type::INT, "surfel_index");

GPU_SHADER_CREATE_INFO(eevee_debug_surfels)
    .additional_info("eevee_shared", "draw_view")
    .vertex_source("eevee_debug_surfels_vert.glsl")
    .vertex_out(eevee_debug_surfel_iface)
    .fragment_source("eevee_debug_surfels_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .storage_buf(0, Qualifier::READ, "Surfel", "surfels_buf[]")
    .push_constant(Type::FLOAT, "debug_surfel_radius")
    .push_constant(Type::INT, "debug_mode")
    .do_static_compilation(true);

GPU_SHADER_INTERFACE_INFO(eevee_debug_irradiance_grid_iface, "")
    .smooth(Type::VEC4, "interp_color");

GPU_SHADER_CREATE_INFO(eevee_debug_irradiance_grid)
    .additional_info("eevee_shared", "draw_view")
    .fragment_out(0, Type::VEC4, "out_color")
    .vertex_out(eevee_debug_irradiance_grid_iface)
    .sampler(0, ImageType::FLOAT_3D, "debug_data_tx")
    .push_constant(Type::MAT4, "grid_mat")
    .push_constant(Type::INT, "debug_mode")
    .push_constant(Type::FLOAT, "debug_value")
    .vertex_source("eevee_debug_irradiance_grid_vert.glsl")
    .fragment_source("eevee_debug_irradiance_grid_frag.glsl")
    .do_static_compilation(true);

GPU_SHADER_INTERFACE_INFO(eevee_display_probe_grid_iface, "")
    .smooth(Type::VEC2, "lP")
    .flat(Type::IVEC3, "cell");

GPU_SHADER_CREATE_INFO(eevee_display_probe_grid)
    .additional_info("eevee_shared", "draw_view")
    .vertex_source("eevee_display_probe_grid_vert.glsl")
    .vertex_out(eevee_display_probe_grid_iface)
    .fragment_source("eevee_display_probe_grid_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .push_constant(Type::FLOAT, "sphere_radius")
    .push_constant(Type::IVEC3, "grid_resolution")
    .push_constant(Type::MAT4, "grid_to_world")
    .push_constant(Type::MAT4, "world_to_grid")
    .push_constant(Type::BOOL, "display_validity")
    .sampler(0, ImageType::FLOAT_3D, "irradiance_a_tx")
    .sampler(1, ImageType::FLOAT_3D, "irradiance_b_tx")
    .sampler(2, ImageType::FLOAT_3D, "irradiance_c_tx")
    .sampler(3, ImageType::FLOAT_3D, "irradiance_d_tx")
    .sampler(4, ImageType::FLOAT_3D, "validity_tx")
    .do_static_compilation(true);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Baking
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_surfel_common)
    .storage_buf(SURFEL_BUF_SLOT, Qualifier::READ_WRITE, "Surfel", "surfel_buf[]")
    .storage_buf(CAPTURE_BUF_SLOT, Qualifier::READ, "CaptureInfoData", "capture_info_buf");

GPU_SHADER_CREATE_INFO(eevee_surfel_light)
    .define("SURFEL_LIGHT")
    .define("LIGHT_ITER_FORCE_NO_CULLING")
    .local_group_size(SURFEL_GROUP_SIZE)
    .additional_info("eevee_shared",
                     "draw_view",
                     "eevee_global_ubo",
                     "eevee_utility_texture",
                     "eevee_surfel_common",
                     "eevee_light_data",
                     "eevee_shadow_data")
    .compute_source("eevee_surfel_light_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_surfel_cluster_build)
    .local_group_size(SURFEL_GROUP_SIZE)
    .builtins(BuiltinBits::TEXTURE_ATOMIC)
    .additional_info("eevee_shared", "eevee_surfel_common", "draw_view")
    .image(0, GPU_R32I, Qualifier::READ_WRITE, ImageType::INT_3D_ATOMIC, "cluster_list_img")
    .compute_source("eevee_surfel_cluster_build_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_surfel_list_build)
    .local_group_size(SURFEL_GROUP_SIZE)
    .builtins(BuiltinBits::TEXTURE_ATOMIC)
    .additional_info("eevee_shared", "eevee_surfel_common", "draw_view")
    .storage_buf(0, Qualifier::READ_WRITE, "int", "list_start_buf[]")
    .storage_buf(6, Qualifier::READ_WRITE, "SurfelListInfoData", "list_info_buf")
    .compute_source("eevee_surfel_list_build_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_surfel_list_sort)
    .local_group_size(SURFEL_LIST_GROUP_SIZE)
    .additional_info("eevee_shared", "eevee_surfel_common", "draw_view")
    .storage_buf(0, Qualifier::READ_WRITE, "int", "list_start_buf[]")
    .storage_buf(6, Qualifier::READ, "SurfelListInfoData", "list_info_buf")
    .compute_source("eevee_surfel_list_sort_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_surfel_ray)
    .local_group_size(SURFEL_GROUP_SIZE)
    .additional_info("eevee_shared",
                     "eevee_surfel_common",
                     "eevee_reflection_probe_data",
                     "draw_view")
    .push_constant(Type::INT, "radiance_src")
    .push_constant(Type::INT, "radiance_dst")
    .compute_source("eevee_surfel_ray_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_lightprobe_irradiance_bounds)
    .do_static_compilation(true)
    .local_group_size(IRRADIANCE_BOUNDS_GROUP_SIZE)
    .storage_buf(0, Qualifier::READ_WRITE, "CaptureInfoData", "capture_info_buf")
    .storage_buf(1, Qualifier::READ, "ObjectBounds", "bounds_buf[]")
    .push_constant(Type::INT, "resource_len")
    .typedef_source("draw_shader_shared.h")
    .additional_info("eevee_shared")
    .compute_source("eevee_lightprobe_irradiance_bounds_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_lightprobe_irradiance_ray)
    .local_group_size(IRRADIANCE_GRID_GROUP_SIZE,
                      IRRADIANCE_GRID_GROUP_SIZE,
                      IRRADIANCE_GRID_GROUP_SIZE)
    .additional_info("eevee_shared",
                     "eevee_surfel_common",
                     "eevee_reflection_probe_data",
                     "draw_view")
    .push_constant(Type::INT, "radiance_src")
    .storage_buf(0, Qualifier::READ, "int", "list_start_buf[]")
    .storage_buf(6, Qualifier::READ, "SurfelListInfoData", "list_info_buf")
    .image(0, GPU_RGBA32F, Qualifier::READ_WRITE, ImageType::FLOAT_3D, "irradiance_L0_img")
    .image(1, GPU_RGBA32F, Qualifier::READ_WRITE, ImageType::FLOAT_3D, "irradiance_L1_a_img")
    .image(2, GPU_RGBA32F, Qualifier::READ_WRITE, ImageType::FLOAT_3D, "irradiance_L1_b_img")
    .image(3, GPU_RGBA32F, Qualifier::READ_WRITE, ImageType::FLOAT_3D, "irradiance_L1_c_img")
    .image(4, GPU_RGBA16F, Qualifier::READ, ImageType::FLOAT_3D, "virtual_offset_img")
    .image(5, GPU_R32F, Qualifier::READ_WRITE, ImageType::FLOAT_3D, "validity_img")
    .compute_source("eevee_lightprobe_irradiance_ray_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_lightprobe_irradiance_offset)
    .local_group_size(IRRADIANCE_GRID_GROUP_SIZE,
                      IRRADIANCE_GRID_GROUP_SIZE,
                      IRRADIANCE_GRID_GROUP_SIZE)
    .additional_info("eevee_shared", "eevee_surfel_common", "draw_view")
    .storage_buf(0, Qualifier::READ, "int", "list_start_buf[]")
    .storage_buf(6, Qualifier::READ, "SurfelListInfoData", "list_info_buf")
    .image(0, GPU_R32I, Qualifier::READ, ImageType::INT_3D_ATOMIC, "cluster_list_img")
    .image(1, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_3D, "virtual_offset_img")
    .compute_source("eevee_lightprobe_irradiance_offset_comp.glsl")
    .do_static_compilation(true);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Runtime
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_lightprobe_irradiance_world)
    .local_group_size(IRRADIANCE_GRID_BRICK_SIZE,
                      IRRADIANCE_GRID_BRICK_SIZE,
                      IRRADIANCE_GRID_BRICK_SIZE)
    .define("IRRADIANCE_GRID_UPLOAD")
    .additional_info("eevee_shared")
    .push_constant(Type::INT, "grid_index")
    .storage_buf(0, Qualifier::READ, "uint", "bricks_infos_buf[]")
    .storage_buf(1, Qualifier::READ, "SphereProbeHarmonic", "harmonic_buf")
    .uniform_buf(0, "VolumeProbeData", "grids_infos_buf[IRRADIANCE_GRID_MAX]")
    .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_3D, "irradiance_atlas_img")
    .compute_source("eevee_lightprobe_irradiance_world_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_lightprobe_irradiance_load)
    .local_group_size(IRRADIANCE_GRID_BRICK_SIZE,
                      IRRADIANCE_GRID_BRICK_SIZE,
                      IRRADIANCE_GRID_BRICK_SIZE)
    .define("IRRADIANCE_GRID_UPLOAD")
    .additional_info("eevee_shared")
    .push_constant(Type::MAT4, "grid_local_to_world")
    .push_constant(Type::INT, "grid_index")
    .push_constant(Type::INT, "grid_start_index")
    .push_constant(Type::FLOAT, "validity_threshold")
    .push_constant(Type::FLOAT, "dilation_threshold")
    .push_constant(Type::FLOAT, "dilation_radius")
    .push_constant(Type::FLOAT, "grid_intensity_factor")
    .uniform_buf(0, "VolumeProbeData", "grids_infos_buf[IRRADIANCE_GRID_MAX]")
    .storage_buf(0, Qualifier::READ, "uint", "bricks_infos_buf[]")
    .sampler(0, ImageType::FLOAT_3D, "irradiance_a_tx")
    .sampler(1, ImageType::FLOAT_3D, "irradiance_b_tx")
    .sampler(2, ImageType::FLOAT_3D, "irradiance_c_tx")
    .sampler(3, ImageType::FLOAT_3D, "irradiance_d_tx")
    .sampler(4, ImageType::FLOAT_3D, "visibility_a_tx")
    .sampler(5, ImageType::FLOAT_3D, "visibility_b_tx")
    .sampler(6, ImageType::FLOAT_3D, "visibility_c_tx")
    .sampler(7, ImageType::FLOAT_3D, "visibility_d_tx")
    .sampler(8, ImageType::FLOAT_3D, "irradiance_atlas_tx")
    .sampler(9, ImageType::FLOAT_3D, "validity_tx")
    .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_3D, "irradiance_atlas_img")
    .compute_source("eevee_lightprobe_irradiance_load_comp.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_volume_probe_data)
    .uniform_buf(IRRADIANCE_GRID_BUF_SLOT,
                 "VolumeProbeData",
                 "grids_infos_buf[IRRADIANCE_GRID_MAX]")
    /* NOTE: Use uint instead of IrradianceBrickPacked because Metal needs to know the exact type.
     */
    .storage_buf(IRRADIANCE_BRICK_BUF_SLOT, Qualifier::READ, "uint", "bricks_infos_buf[]")
    .sampler(VOLUME_PROBE_TEX_SLOT, ImageType::FLOAT_3D, "irradiance_atlas_tx")
    .define("IRRADIANCE_GRID_SAMPLING");

GPU_SHADER_CREATE_INFO(eevee_lightprobe_data)
    .additional_info("eevee_reflection_probe_data", "eevee_volume_probe_data");

GPU_SHADER_CREATE_INFO(eevee_lightprobe_planar_data)
    .define("SPHERE_PROBE")
    .uniform_buf(PLANAR_PROBE_BUF_SLOT, "PlanarProbeData", "probe_planar_buf[PLANAR_PROBE_MAX]")
    .sampler(PLANAR_PROBE_RADIANCE_TEX_SLOT, ImageType::FLOAT_2D_ARRAY, "planar_radiance_tx")
    .sampler(PLANAR_PROBE_DEPTH_TEX_SLOT, ImageType::DEPTH_2D_ARRAY, "planar_depth_tx");

/** \} */
