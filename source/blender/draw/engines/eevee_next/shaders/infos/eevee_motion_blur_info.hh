/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_motion_blur_tiles_flatten)
LOCAL_GROUP_SIZE(MOTION_BLUR_GROUP_SIZE, MOTION_BLUR_GROUP_SIZE)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_velocity_camera)
UNIFORM_BUF(6, MotionBlurData, motion_blur_buf)
SAMPLER(0, DEPTH_2D, depth_tx)
IMAGE(1, GPU_RGBA16F, WRITE, FLOAT_2D, out_tiles_img)
COMPUTE_SOURCE("eevee_motion_blur_flatten_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_motion_blur_tiles_flatten_rg)
DO_STATIC_COMPILATION()
DEFINE("FLATTEN_RG")
IMAGE(0, GPU_RG16F, READ_WRITE, FLOAT_2D, velocity_img)
ADDITIONAL_INFO(eevee_motion_blur_tiles_flatten)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_motion_blur_tiles_flatten_rgba)
DO_STATIC_COMPILATION()
IMAGE(0, GPU_RGBA16F, READ_WRITE, FLOAT_2D, velocity_img)
ADDITIONAL_INFO(eevee_motion_blur_tiles_flatten)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_motion_blur_tiles_dilate)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(MOTION_BLUR_GROUP_SIZE, MOTION_BLUR_GROUP_SIZE)
ADDITIONAL_INFO(eevee_shared)
/* NOTE: See MotionBlurTileIndirection. */
STORAGE_BUF(0, READ_WRITE, uint, tile_indirection_buf[])
IMAGE(1, GPU_RGBA16F, READ, FLOAT_2D, in_tiles_img)
COMPUTE_SOURCE("eevee_motion_blur_dilate_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_motion_blur_gather)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(MOTION_BLUR_GROUP_SIZE, MOTION_BLUR_GROUP_SIZE)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_sampling_data)
UNIFORM_BUF(6, MotionBlurData, motion_blur_buf)
SAMPLER(0, DEPTH_2D, depth_tx)
SAMPLER(1, FLOAT_2D, velocity_tx)
SAMPLER(2, FLOAT_2D, in_color_tx)
/* NOTE: See MotionBlurTileIndirection. */
STORAGE_BUF(0, READ, uint, tile_indirection_buf[])
IMAGE(0, GPU_RGBA16F, READ, FLOAT_2D, in_tiles_img)
IMAGE(1, GPU_RGBA16F, WRITE, FLOAT_2D, out_color_img)
COMPUTE_SOURCE("eevee_motion_blur_gather_comp.glsl")
GPU_SHADER_CREATE_END()
