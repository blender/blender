/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_motion_blur_tiles_flatten)
    .local_group_size(MOTION_BLUR_GROUP_SIZE, MOTION_BLUR_GROUP_SIZE)
    .additional_info("eevee_shared", "draw_view", "eevee_velocity_camera")
    .uniform_buf(6, "MotionBlurData", "motion_blur_buf")
    .sampler(0, ImageType::DEPTH_2D, "depth_tx")
    .image(1, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_tiles_img")
    .compute_source("eevee_motion_blur_flatten_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_motion_blur_tiles_flatten_rg)
    .do_static_compilation(true)
    .define("FLATTEN_RG")
    .image(0, GPU_RG16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "velocity_img")
    .additional_info("eevee_motion_blur_tiles_flatten");

GPU_SHADER_CREATE_INFO(eevee_motion_blur_tiles_flatten_rgba)
    .do_static_compilation(true)
    .image(0, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "velocity_img")
    .additional_info("eevee_motion_blur_tiles_flatten");

GPU_SHADER_CREATE_INFO(eevee_motion_blur_tiles_dilate)
    .do_static_compilation(true)
    .local_group_size(MOTION_BLUR_GROUP_SIZE, MOTION_BLUR_GROUP_SIZE)
    .additional_info("eevee_shared")
    /* NOTE: See MotionBlurTileIndirection. */
    .storage_buf(0, Qualifier::READ_WRITE, "uint", "tile_indirection_buf[]")
    .image(1, GPU_RGBA16F, Qualifier::READ, ImageType::FLOAT_2D, "in_tiles_img")
    .compute_source("eevee_motion_blur_dilate_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_motion_blur_gather)
    .do_static_compilation(true)
    .local_group_size(MOTION_BLUR_GROUP_SIZE, MOTION_BLUR_GROUP_SIZE)
    .additional_info("eevee_shared", "draw_view", "eevee_sampling_data")
    .uniform_buf(6, "MotionBlurData", "motion_blur_buf")
    .sampler(0, ImageType::DEPTH_2D, "depth_tx")
    .sampler(1, ImageType::FLOAT_2D, "velocity_tx")
    .sampler(2, ImageType::FLOAT_2D, "in_color_tx")
    /* NOTE: See MotionBlurTileIndirection. */
    .storage_buf(0, Qualifier::READ, "uint", "tile_indirection_buf[]")
    .image(0, GPU_RGBA16F, Qualifier::READ, ImageType::FLOAT_2D, "in_tiles_img")
    .image(1, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_color_img")
    .compute_source("eevee_motion_blur_gather_comp.glsl");
