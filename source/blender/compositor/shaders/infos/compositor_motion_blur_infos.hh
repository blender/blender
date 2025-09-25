/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_motion_blur_max_velocity_dilate)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float, shutter_speed)
SAMPLER(0, sampler2D, input_tx)
STORAGE_BUF(0, read_write, uint, tile_indirection_buf[])
COMPUTE_SOURCE("compositor_motion_blur_max_velocity_dilate.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_motion_blur)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(int, samples_count)
PUSH_CONSTANT(float, shutter_speed)
SAMPLER(0, sampler2D, input_tx)
SAMPLER(1, sampler2D, depth_tx)
SAMPLER(2, sampler2D, velocity_tx)
SAMPLER(3, sampler2D, max_velocity_tx)
STORAGE_BUF(0, read, uint, tile_indirection_buf[])
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_motion_blur.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
