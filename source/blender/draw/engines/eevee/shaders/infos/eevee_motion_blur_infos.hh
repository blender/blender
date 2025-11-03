/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"
#  include "eevee_motion_blur_shared.hh"
#  include "eevee_sampling_infos.hh"
#  include "eevee_velocity_infos.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_motion_blur_tiles_flatten)
LOCAL_GROUP_SIZE(MOTION_BLUR_GROUP_SIZE, MOTION_BLUR_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_motion_blur_shared.hh")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_velocity_camera)
UNIFORM_BUF(6, MotionBlurData, motion_blur_buf)
SAMPLER(0, sampler2DDepth, depth_tx)
IMAGE(1, SFLOAT_16_16_16_16, write, image2D, out_tiles_img)
COMPUTE_SOURCE("eevee_motion_blur_flatten_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_motion_blur_tiles_flatten_rg)
DO_STATIC_COMPILATION()
DEFINE("FLATTEN_RG")
IMAGE(0, SFLOAT_16_16, read_write, image2D, velocity_img)
ADDITIONAL_INFO(eevee_motion_blur_tiles_flatten)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_motion_blur_tiles_flatten_rgba)
DO_STATIC_COMPILATION()
IMAGE(0, SFLOAT_16_16_16_16, read_write, image2D, velocity_img)
ADDITIONAL_INFO(eevee_motion_blur_tiles_flatten)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_motion_blur_tiles_dilate)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(MOTION_BLUR_GROUP_SIZE, MOTION_BLUR_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_motion_blur_shared.hh")
/* NOTE: See MotionBlurTileIndirection. */
STORAGE_BUF(0, read_write, uint, tile_indirection_buf[])
IMAGE(1, SFLOAT_16_16_16_16, read, image2D, in_tiles_img)
COMPUTE_SOURCE("eevee_motion_blur_dilate_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_motion_blur_gather)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(MOTION_BLUR_GROUP_SIZE, MOTION_BLUR_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_motion_blur_shared.hh")
TYPEDEF_SOURCE("eevee_camera_shared.hh")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_sampling_data)
UNIFORM_BUF(6, MotionBlurData, motion_blur_buf)
SAMPLER(0, sampler2DDepth, depth_tx)
SAMPLER(1, sampler2D, velocity_tx)
SAMPLER(2, sampler2D, in_color_tx)
/* NOTE: See MotionBlurTileIndirection. */
STORAGE_BUF(0, read, uint, tile_indirection_buf[])
IMAGE(0, SFLOAT_16_16_16_16, read, image2D, in_tiles_img)
IMAGE(1, SFLOAT_16_16_16_16, write, image2D, out_color_img)
COMPUTE_SOURCE("eevee_motion_blur_gather_comp.glsl")
GPU_SHADER_CREATE_END()
