/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Setup
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_bokeh_lut)
    .do_static_compilation(true)
    .local_group_size(DOF_BOKEH_LUT_SIZE, DOF_BOKEH_LUT_SIZE)
    .additional_info("eevee_shared", "draw_view")
    .uniform_buf(6, "DepthOfFieldData", "dof_buf")
    .image(0, GPU_RG16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_gather_lut_img")
    .image(1, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_scatter_lut_img")
    .image(2, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_resolve_lut_img")
    .compute_source("eevee_depth_of_field_bokeh_lut_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_setup)
    .do_static_compilation(true)
    .local_group_size(DOF_DEFAULT_GROUP_SIZE, DOF_DEFAULT_GROUP_SIZE)
    .additional_info("eevee_shared", "draw_view")
    .uniform_buf(6, "DepthOfFieldData", "dof_buf")
    .sampler(0, ImageType::FLOAT_2D, "color_tx")
    .sampler(1, ImageType::DEPTH_2D, "depth_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_color_img")
    .image(1, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_coc_img")
    .compute_source("eevee_depth_of_field_setup_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_stabilize)
    .do_static_compilation(true)
    .local_group_size(DOF_STABILIZE_GROUP_SIZE, DOF_STABILIZE_GROUP_SIZE)
    .additional_info("eevee_shared", "draw_view", "eevee_velocity_camera")
    .uniform_buf(6, "DepthOfFieldData", "dof_buf")
    .sampler(0, ImageType::FLOAT_2D, "coc_tx")
    .sampler(1, ImageType::FLOAT_2D, "color_tx")
    .sampler(2, ImageType::FLOAT_2D, "velocity_tx")
    .sampler(3, ImageType::FLOAT_2D, "in_history_tx")
    .sampler(4, ImageType::DEPTH_2D, "depth_tx")
    .push_constant(Type::BOOL, "u_use_history")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_color_img")
    .image(1, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_coc_img")
    .image(2, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_history_img")
    .compute_source("eevee_depth_of_field_stabilize_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_downsample)
    .do_static_compilation(true)
    .local_group_size(DOF_DEFAULT_GROUP_SIZE, DOF_DEFAULT_GROUP_SIZE)
    .additional_info("eevee_shared", "draw_view")
    .sampler(0, ImageType::FLOAT_2D, "color_tx")
    .sampler(1, ImageType::FLOAT_2D, "coc_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_color_img")
    .compute_source("eevee_depth_of_field_downsample_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_reduce)
    .do_static_compilation(true)
    .local_group_size(DOF_REDUCE_GROUP_SIZE, DOF_REDUCE_GROUP_SIZE)
    .additional_info("eevee_shared", "draw_view")
    .uniform_buf(6, "DepthOfFieldData", "dof_buf")
    .sampler(0, ImageType::FLOAT_2D, "downsample_tx")
    .storage_buf(0, Qualifier::WRITE, "ScatterRect", "scatter_fg_list_buf[]")
    .storage_buf(1, Qualifier::WRITE, "ScatterRect", "scatter_bg_list_buf[]")
    .storage_buf(2, Qualifier::READ_WRITE, "DrawCommand", "scatter_fg_indirect_buf")
    .storage_buf(3, Qualifier::READ_WRITE, "DrawCommand", "scatter_bg_indirect_buf")
    .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "inout_color_lod0_img")
    .image(1, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_color_lod1_img")
    .image(2, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_color_lod2_img")
    .image(3, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_color_lod3_img")
    .image(4, GPU_R16F, Qualifier::READ, ImageType::FLOAT_2D, "in_coc_lod0_img")
    .image(5, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_coc_lod1_img")
    .image(6, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_coc_lod2_img")
    .image(7, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_coc_lod3_img")
    .compute_source("eevee_depth_of_field_reduce_comp.glsl");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle-Of-Confusion Tiles
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_tiles_flatten)
    .do_static_compilation(true)
    .local_group_size(DOF_TILES_FLATTEN_GROUP_SIZE, DOF_TILES_FLATTEN_GROUP_SIZE)
    .additional_info("eevee_shared", "draw_view")
    .sampler(0, ImageType::FLOAT_2D, "coc_tx")
    .image(2, GPU_R11F_G11F_B10F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_tiles_fg_img")
    .image(3, GPU_R11F_G11F_B10F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_tiles_bg_img")
    .compute_source("eevee_depth_of_field_tiles_flatten_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_tiles_dilate)
    .additional_info("eevee_shared", "draw_view", "eevee_depth_of_field_tiles_common")
    .local_group_size(DOF_TILES_DILATE_GROUP_SIZE, DOF_TILES_DILATE_GROUP_SIZE)
    .image(2, GPU_R11F_G11F_B10F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_tiles_fg_img")
    .image(3, GPU_R11F_G11F_B10F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_tiles_bg_img")
    .push_constant(Type::INT, "ring_count")
    .push_constant(Type::INT, "ring_width_multiplier")
    .compute_source("eevee_depth_of_field_tiles_dilate_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_tiles_dilate_minabs)
    .do_static_compilation(true)
    .define("DILATE_MODE_MIN_MAX", "false")
    .additional_info("eevee_depth_of_field_tiles_dilate");

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_tiles_dilate_minmax)
    .do_static_compilation(true)
    .define("DILATE_MODE_MIN_MAX", "true")
    .additional_info("eevee_depth_of_field_tiles_dilate");

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_tiles_common)
    .image(0, GPU_R11F_G11F_B10F, Qualifier::READ, ImageType::FLOAT_2D, "in_tiles_fg_img")
    .image(1, GPU_R11F_G11F_B10F, Qualifier::READ, ImageType::FLOAT_2D, "in_tiles_bg_img");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Variations
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_no_lut)
    .define("DOF_BOKEH_TEXTURE", "false")
    /**
     * WORKAROUND(@fclem): This is to keep the code as is for now. The bokeh_lut_tx is referenced
     * even if not used after optimization. But we don't want to include it in the create infos.
     */
    .define("bokeh_lut_tx", "color_tx");

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_lut)
    .define("DOF_BOKEH_TEXTURE", "true")
    .sampler(5, ImageType::FLOAT_2D, "bokeh_lut_tx");

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_background).define("DOF_FOREGROUND_PASS", "false");
GPU_SHADER_CREATE_INFO(eevee_depth_of_field_foreground).define("DOF_FOREGROUND_PASS", "true");

#define EEVEE_DOF_FINAL_VARIATION(name, ...) \
  GPU_SHADER_CREATE_INFO(name).additional_info(__VA_ARGS__).do_static_compilation(true);

#define EEVEE_DOF_LUT_VARIATIONS(prefix, ...) \
  EEVEE_DOF_FINAL_VARIATION(prefix##_lut, "eevee_depth_of_field_lut", __VA_ARGS__) \
  EEVEE_DOF_FINAL_VARIATION(prefix##_no_lut, "eevee_depth_of_field_no_lut", __VA_ARGS__)

#define EEVEE_DOF_GROUND_VARIATIONS(name, ...) \
  EEVEE_DOF_LUT_VARIATIONS(name##_background, "eevee_depth_of_field_background", __VA_ARGS__) \
  EEVEE_DOF_LUT_VARIATIONS(name##_foreground, "eevee_depth_of_field_foreground", __VA_ARGS__)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gather
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_gather_common)
    .additional_info("eevee_shared",
                     "draw_view",
                     "eevee_depth_of_field_tiles_common",
                     "eevee_sampling_data")
    .uniform_buf(6, "DepthOfFieldData", "dof_buf")
    .local_group_size(DOF_GATHER_GROUP_SIZE, DOF_GATHER_GROUP_SIZE)
    .sampler(0, ImageType::FLOAT_2D, "color_tx")
    .sampler(1, ImageType::FLOAT_2D, "color_bilinear_tx")
    .sampler(2, ImageType::FLOAT_2D, "coc_tx")
    .image(2, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_color_img")
    .image(3, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_weight_img");

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_gather)
    .image(4, GPU_RG16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_occlusion_img")
    .compute_source("eevee_depth_of_field_gather_comp.glsl")
    .additional_info("eevee_depth_of_field_gather_common");

EEVEE_DOF_GROUND_VARIATIONS(eevee_depth_of_field_gather, "eevee_depth_of_field_gather")

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_hole_fill)
    .do_static_compilation(true)
    .compute_source("eevee_depth_of_field_hole_fill_comp.glsl")
    .additional_info("eevee_depth_of_field_gather_common", "eevee_depth_of_field_no_lut");

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_filter)
    .do_static_compilation(true)
    .local_group_size(DOF_FILTER_GROUP_SIZE, DOF_FILTER_GROUP_SIZE)
    .additional_info("eevee_shared")
    .sampler(0, ImageType::FLOAT_2D, "color_tx")
    .sampler(1, ImageType::FLOAT_2D, "weight_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_color_img")
    .image(1, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_weight_img")
    .compute_source("eevee_depth_of_field_filter_comp.glsl");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scatter
 * \{ */

GPU_SHADER_INTERFACE_INFO(eevee_depth_of_field_scatter_iface, "interp")
    /** Colors, weights, and Circle of confusion radii for the 4 pixels to scatter. */
    .flat(Type::VEC4, "color_and_coc1")
    .flat(Type::VEC4, "color_and_coc2")
    .flat(Type::VEC4, "color_and_coc3")
    .flat(Type::VEC4, "color_and_coc4")
    /** Sprite pixel position with origin at sprite center. In pixels. */
    .no_perspective(Type::VEC2, "rect_uv1")
    .no_perspective(Type::VEC2, "rect_uv2")
    .no_perspective(Type::VEC2, "rect_uv3")
    .no_perspective(Type::VEC2, "rect_uv4")
    /** Scaling factor for the bokeh distance. */
    .flat(Type::FLOAT, "distance_scale");

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_scatter)
    .do_static_compilation(true)
    .additional_info("eevee_shared", "draw_view")
    .sampler(0, ImageType::FLOAT_2D, "occlusion_tx")
    .sampler(1, ImageType::FLOAT_2D, "bokeh_lut_tx")
    .storage_buf(0, Qualifier::READ, "ScatterRect", "scatter_list_buf[]")
    .fragment_out(0, Type::VEC4, "out_color")
    .push_constant(Type::BOOL, "use_bokeh_lut")
    .vertex_out(eevee_depth_of_field_scatter_iface)
    .vertex_source("eevee_depth_of_field_scatter_vert.glsl")
    .fragment_source("eevee_depth_of_field_scatter_frag.glsl");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Resolve
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_resolve)
    .define("DOF_RESOLVE_PASS", "true")
    .local_group_size(DOF_RESOLVE_GROUP_SIZE, DOF_RESOLVE_GROUP_SIZE)
    .additional_info("eevee_shared",
                     "draw_view",
                     "eevee_depth_of_field_tiles_common",
                     "eevee_sampling_data")
    .uniform_buf(6, "DepthOfFieldData", "dof_buf")
    .sampler(0, ImageType::DEPTH_2D, "depth_tx")
    .sampler(1, ImageType::FLOAT_2D, "color_tx")
    .sampler(2, ImageType::FLOAT_2D, "color_bg_tx")
    .sampler(3, ImageType::FLOAT_2D, "color_fg_tx")
    .sampler(4, ImageType::FLOAT_2D, "color_hole_fill_tx")
    .sampler(7, ImageType::FLOAT_2D, "weight_bg_tx")
    .sampler(8, ImageType::FLOAT_2D, "weight_fg_tx")
    .sampler(9, ImageType::FLOAT_2D, "weight_hole_fill_tx")
    .sampler(10, ImageType::FLOAT_2D, "stable_color_tx")
    .image(2, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_color_img")
    .compute_source("eevee_depth_of_field_resolve_comp.glsl");

EEVEE_DOF_LUT_VARIATIONS(eevee_depth_of_field_resolve, "eevee_depth_of_field_resolve")

/** \} */
