/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Ray tracing Pipeline
 * \{ */

#define image_out(slot, format, type, name) \
  image(slot, format, Qualifier::WRITE, type, name, Frequency::PASS)
#define image_in(slot, format, type, name) \
  image(slot, format, Qualifier::READ, type, name, Frequency::PASS)

GPU_SHADER_CREATE_INFO(eevee_ray_tile_classify)
    .do_static_compilation(true)
    .local_group_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
    .additional_info("eevee_shared", "eevee_gbuffer_data", "eevee_global_ubo")
    .typedef_source("draw_shader_shared.hh")
    .image_out(0, RAYTRACE_TILEMASK_FORMAT, ImageType::UINT_2D_ARRAY, "tile_raytrace_denoise_img")
    .image_out(1, RAYTRACE_TILEMASK_FORMAT, ImageType::UINT_2D_ARRAY, "tile_raytrace_tracing_img")
    .image_out(2, RAYTRACE_TILEMASK_FORMAT, ImageType::UINT_2D_ARRAY, "tile_horizon_denoise_img")
    .image_out(3, RAYTRACE_TILEMASK_FORMAT, ImageType::UINT_2D_ARRAY, "tile_horizon_tracing_img")
    .compute_source("eevee_ray_tile_classify_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_ray_tile_compact)
    .do_static_compilation(true)
    .local_group_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
    .additional_info("eevee_shared", "eevee_global_ubo")
    .typedef_source("draw_shader_shared.hh")
    .image_in(0, RAYTRACE_TILEMASK_FORMAT, ImageType::UINT_2D_ARRAY, "tile_raytrace_denoise_img")
    .image_in(1, RAYTRACE_TILEMASK_FORMAT, ImageType::UINT_2D_ARRAY, "tile_raytrace_tracing_img")
    .storage_buf(0, Qualifier::READ_WRITE, "DispatchCommand", "raytrace_tracing_dispatch_buf")
    .storage_buf(1, Qualifier::READ_WRITE, "DispatchCommand", "raytrace_denoise_dispatch_buf")
    .storage_buf(4, Qualifier::WRITE, "uint", "raytrace_tracing_tiles_buf[]")
    .storage_buf(5, Qualifier::WRITE, "uint", "raytrace_denoise_tiles_buf[]")
    .specialization_constant(Type::INT, "closure_index", 0)
    .specialization_constant(Type::INT, "resolution_scale", 2)
    .compute_source("eevee_ray_tile_compact_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_ray_generate)
    .do_static_compilation(true)
    .local_group_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
    .additional_info("eevee_shared",
                     "eevee_gbuffer_data",
                     "eevee_global_ubo",
                     "eevee_sampling_data",
                     "draw_view",
                     "eevee_utility_texture")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_ray_data_img")
    .storage_buf(4, Qualifier::READ, "uint", "tiles_coord_buf[]")
    .specialization_constant(Type::INT, "closure_index", 0)
    .compute_source("eevee_ray_generate_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_ray_trace_fallback)
    .do_static_compilation(true)
    .local_group_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
    .additional_info("eevee_shared",
                     "eevee_gbuffer_data",
                     "eevee_global_ubo",
                     "draw_view",
                     "eevee_sampling_data",
                     "eevee_lightprobe_data")
    .image(0, GPU_RGBA16F, Qualifier::READ, ImageType::FLOAT_2D, "ray_data_img")
    .image(1, RAYTRACE_RAYTIME_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "ray_time_img")
    .image(2, RAYTRACE_RADIANCE_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "ray_radiance_img")
    .sampler(1, ImageType::DEPTH_2D, "depth_tx")
    .storage_buf(5, Qualifier::READ, "uint", "tiles_coord_buf[]")
    .specialization_constant(Type::INT, "closure_index", 0)
    .compute_source("eevee_ray_trace_fallback_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_ray_trace_planar)
    .do_static_compilation(true)
    .local_group_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
    .define("PLANAR_PROBES")
    .additional_info("eevee_shared",
                     "eevee_global_ubo",
                     "eevee_sampling_data",
                     "eevee_gbuffer_data",
                     "draw_view",
                     "eevee_lightprobe_data",
                     "eevee_lightprobe_planar_data")
    .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "ray_data_img")
    .image(1, RAYTRACE_RAYTIME_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "ray_time_img")
    .image(2, RAYTRACE_RADIANCE_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "ray_radiance_img")
    .sampler(2, ImageType::DEPTH_2D, "depth_tx")
    .storage_buf(5, Qualifier::READ, "uint", "tiles_coord_buf[]")
    .specialization_constant(Type::INT, "closure_index", 0)
    .compute_source("eevee_ray_trace_planar_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_ray_trace_screen)
    .do_static_compilation(true)
    .local_group_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
    .additional_info("eevee_shared",
                     "eevee_global_ubo",
                     "eevee_sampling_data",
                     "eevee_gbuffer_data",
                     "draw_view",
                     "eevee_hiz_data",
                     "eevee_lightprobe_data")
    .image(0, GPU_RGBA16F, Qualifier::READ, ImageType::FLOAT_2D, "ray_data_img")
    .image(1, RAYTRACE_RAYTIME_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "ray_time_img")
    .image(2, RAYTRACE_RADIANCE_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "ray_radiance_img")
    .sampler(0, ImageType::DEPTH_2D, "depth_tx")
    .sampler(1, ImageType::FLOAT_2D, "radiance_front_tx")
    .sampler(2, ImageType::FLOAT_2D, "radiance_back_tx")
    .sampler(4, ImageType::FLOAT_2D, "hiz_front_tx")
    .sampler(5, ImageType::FLOAT_2D, "hiz_back_tx")
    .storage_buf(5, Qualifier::READ, "uint", "tiles_coord_buf[]")
    .compute_source("eevee_ray_trace_screen_comp.glsl")
    /* Metal: Provide compiler with hint to tune per-thread resource allocation. */
    .mtl_max_total_threads_per_threadgroup(400)
    .specialization_constant(Type::BOOL, "trace_refraction", true)
    .specialization_constant(Type::INT, "closure_index", 0)
    .compute_source("eevee_ray_trace_screen_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_ray_denoise_spatial)
    .do_static_compilation(true)
    .local_group_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
    .additional_info("eevee_shared",
                     "eevee_gbuffer_data",
                     "eevee_global_ubo",
                     "eevee_sampling_data",
                     "draw_view",
                     "eevee_utility_texture")
    .sampler(3, ImageType::DEPTH_2D, "depth_tx")
    .image(0, GPU_RGBA16F, Qualifier::READ, ImageType::FLOAT_2D, "ray_data_img")
    .image(1, RAYTRACE_RAYTIME_FORMAT, Qualifier::READ, ImageType::FLOAT_2D, "ray_time_img")
    .image(2, RAYTRACE_RADIANCE_FORMAT, Qualifier::READ, ImageType::FLOAT_2D, "ray_radiance_img")
    .image(3, RAYTRACE_RADIANCE_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "out_radiance_img")
    .image(4, RAYTRACE_VARIANCE_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "out_variance_img")
    .image(5, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_hit_depth_img")
    .image(6, RAYTRACE_TILEMASK_FORMAT, Qualifier::READ, ImageType::UINT_2D_ARRAY, "tile_mask_img")
    .storage_buf(4, Qualifier::READ, "uint", "tiles_coord_buf[]")
    /* Metal: Provide compiler with hint to tune per-thread resource allocation. */
    .mtl_max_total_threads_per_threadgroup(316)
    .specialization_constant(Type::INT, "raytrace_resolution_scale", 2)
    .specialization_constant(Type::BOOL, "skip_denoise", false)
    .specialization_constant(Type::INT, "closure_index", 0)
    .compute_source("eevee_ray_denoise_spatial_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_ray_denoise_temporal)
    .do_static_compilation(true)
    .local_group_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
    .additional_info("eevee_shared", "eevee_global_ubo", "draw_view")
    .sampler(0, ImageType::FLOAT_2D, "radiance_history_tx")
    .sampler(1, ImageType::FLOAT_2D, "variance_history_tx")
    .sampler(2, ImageType::UINT_2D_ARRAY, "tilemask_history_tx")
    .sampler(3, ImageType::DEPTH_2D, "depth_tx")
    .image(0, GPU_R32F, Qualifier::READ, ImageType::FLOAT_2D, "hit_depth_img")
    .image(1, RAYTRACE_RADIANCE_FORMAT, Qualifier::READ, ImageType::FLOAT_2D, "in_radiance_img")
    .image(2, RAYTRACE_RADIANCE_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "out_radiance_img")
    .image(3, RAYTRACE_VARIANCE_FORMAT, Qualifier::READ, ImageType::FLOAT_2D, "in_variance_img")
    .image(4, RAYTRACE_VARIANCE_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "out_variance_img")
    .storage_buf(4, Qualifier::READ, "uint", "tiles_coord_buf[]")
    .compute_source("eevee_ray_denoise_temporal_comp.glsl")
    /* Metal: Provide compiler with hint to tune per-thread resource allocation. */
    .mtl_max_total_threads_per_threadgroup(512)
    .specialization_constant(Type::INT, "closure_index", 0)
    .compute_source("eevee_ray_denoise_temporal_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_ray_denoise_bilateral)
    .do_static_compilation(true)
    .local_group_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
    .additional_info("eevee_shared",
                     "eevee_gbuffer_data",
                     "eevee_global_ubo",
                     "eevee_sampling_data",
                     "draw_view")
    .sampler(1, ImageType::DEPTH_2D, "depth_tx")
    .image(1, RAYTRACE_RADIANCE_FORMAT, Qualifier::READ, ImageType::FLOAT_2D, "in_radiance_img")
    .image(2, RAYTRACE_RADIANCE_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "out_radiance_img")
    .image(3, RAYTRACE_VARIANCE_FORMAT, Qualifier::READ, ImageType::FLOAT_2D, "in_variance_img")
    .image(6, RAYTRACE_TILEMASK_FORMAT, Qualifier::READ, ImageType::UINT_2D_ARRAY, "tile_mask_img")
    .storage_buf(4, Qualifier::READ, "uint", "tiles_coord_buf[]")
    .specialization_constant(Type::INT, "closure_index", 0)
    .compute_source("eevee_ray_denoise_bilateral_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_horizon_setup)
    .do_static_compilation(true)
    .local_group_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
    .additional_info("eevee_shared", "eevee_gbuffer_data", "eevee_global_ubo", "draw_view")
    .sampler(0, ImageType::DEPTH_2D, "depth_tx")
    .sampler(1, ImageType::FLOAT_2D, "in_radiance_tx")
    .image(2, RAYTRACE_RADIANCE_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "out_radiance_img")
    .image(3, GPU_RGB10_A2, Qualifier::WRITE, ImageType::FLOAT_2D, "out_normal_img")
    .compute_source("eevee_horizon_setup_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_horizon_scan)
    .do_static_compilation(true)
    .local_group_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
    .additional_info("eevee_shared",
                     "eevee_gbuffer_data",
                     "eevee_global_ubo",
                     "eevee_sampling_data",
                     "eevee_utility_texture",
                     "eevee_hiz_data",
                     "draw_view")
    .sampler(0, ImageType::FLOAT_2D, "screen_radiance_tx")
    .sampler(1, ImageType::FLOAT_2D, "screen_normal_tx")
    .image(2, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "horizon_radiance_0_img")
    .image(3, GPU_RGBA8, Qualifier::WRITE, ImageType::FLOAT_2D, "horizon_radiance_1_img")
    .image(4, GPU_RGBA8, Qualifier::WRITE, ImageType::FLOAT_2D, "horizon_radiance_2_img")
    .image(5, GPU_RGBA8, Qualifier::WRITE, ImageType::FLOAT_2D, "horizon_radiance_3_img")
    .storage_buf(7, Qualifier::READ, "uint", "tiles_coord_buf[]")
    /* Metal: Provide compiler with hint to tune per-thread resource allocation. */
    .mtl_max_total_threads_per_threadgroup(400)
    .compute_source("eevee_horizon_scan_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_horizon_denoise)
    .do_static_compilation(true)
    .local_group_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
    .additional_info(
        "eevee_shared", "eevee_global_ubo", "eevee_sampling_data", "eevee_hiz_data", "draw_view")
    .sampler(2, ImageType::FLOAT_2D, "in_sh_0_tx")
    .sampler(4, ImageType::FLOAT_2D, "in_sh_1_tx")
    .sampler(5, ImageType::FLOAT_2D, "in_sh_2_tx")
    .sampler(6, ImageType::FLOAT_2D, "in_sh_3_tx")
    .sampler(7, ImageType::FLOAT_2D, "screen_normal_tx")
    .image(2, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_sh_0_img")
    .image(3, GPU_RGBA8, Qualifier::WRITE, ImageType::FLOAT_2D, "out_sh_1_img")
    .image(4, GPU_RGBA8, Qualifier::WRITE, ImageType::FLOAT_2D, "out_sh_2_img")
    .image(5, GPU_RGBA8, Qualifier::WRITE, ImageType::FLOAT_2D, "out_sh_3_img")
    .storage_buf(7, Qualifier::READ, "uint", "tiles_coord_buf[]")
    .compute_source("eevee_horizon_denoise_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_horizon_resolve)
    .do_static_compilation(true)
    .local_group_size(RAYTRACE_GROUP_SIZE, RAYTRACE_GROUP_SIZE)
    .additional_info("eevee_shared",
                     "eevee_gbuffer_data",
                     "eevee_global_ubo",
                     "eevee_sampling_data",
                     "eevee_lightprobe_data",
                     "draw_view")
    .sampler(1, ImageType::DEPTH_2D, "depth_tx")
    .sampler(2, ImageType::FLOAT_2D, "horizon_radiance_0_tx")
    .sampler(3, ImageType::FLOAT_2D, "horizon_radiance_1_tx")
    .sampler(4, ImageType::FLOAT_2D, "horizon_radiance_2_tx")
    .sampler(5, ImageType::FLOAT_2D, "horizon_radiance_3_tx")
    .sampler(8, ImageType::FLOAT_2D, "screen_normal_tx")
    .image(3, RAYTRACE_RADIANCE_FORMAT, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "closure0_img")
    .image(4, RAYTRACE_RADIANCE_FORMAT, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "closure1_img")
    .image(5, RAYTRACE_RADIANCE_FORMAT, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "closure2_img")
    .storage_buf(7, Qualifier::READ, "uint", "tiles_coord_buf[]")
    /* Metal: Provide compiler with hint to tune per-thread resource allocation. */
    .mtl_max_total_threads_per_threadgroup(400)
    .compute_source("eevee_horizon_resolve_comp.glsl");

#undef image_out
#undef image_in

/** \} */
