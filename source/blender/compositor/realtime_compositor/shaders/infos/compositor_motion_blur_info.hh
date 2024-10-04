/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_motion_blur_max_velocity_dilate)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(FLOAT, shutter_speed)
SAMPLER(0, FLOAT_2D, input_tx)
STORAGE_BUF(0, READ_WRITE, uint, tile_indirection_buf[])
COMPUTE_SOURCE("compositor_motion_blur_max_velocity_dilate.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_motion_blur)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(INT, samples_count)
PUSH_CONSTANT(FLOAT, shutter_speed)
SAMPLER(0, FLOAT_2D, input_tx)
SAMPLER(1, FLOAT_2D, depth_tx)
SAMPLER(2, FLOAT_2D, velocity_tx)
SAMPLER(3, FLOAT_2D, max_velocity_tx)
STORAGE_BUF(0, READ, uint, tile_indirection_buf[])
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_motion_blur.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
