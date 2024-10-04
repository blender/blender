/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_plane_deform_mask)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(MAT4, homography_matrix)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, mask_img)
COMPUTE_SOURCE("compositor_plane_deform_mask.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_plane_deform)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(MAT4, homography_matrix)
SAMPLER(0, FLOAT_2D, input_tx)
SAMPLER(1, FLOAT_2D, mask_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_plane_deform.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_plane_deform_motion_blur_mask)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(INT, number_of_motion_blur_samples)
UNIFORM_BUF(0, mat4, homography_matrices[64])
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, mask_img)
COMPUTE_SOURCE("compositor_plane_deform_motion_blur_mask.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_plane_deform_motion_blur)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(INT, number_of_motion_blur_samples)
UNIFORM_BUF(0, mat4, homography_matrices[64])
SAMPLER(0, FLOAT_2D, input_tx)
SAMPLER(1, FLOAT_2D, mask_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_plane_deform_motion_blur.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
