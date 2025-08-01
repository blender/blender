/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_incomplete_prologues_shared)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_32_32_32_32, write, image2D, incomplete_x_prologues_img)
IMAGE(1, SFLOAT_32_32_32_32, write, image2D, incomplete_y_prologues_img)
COMPUTE_SOURCE("compositor_summed_area_table_compute_incomplete_prologues.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_incomplete_prologues_identity)
ADDITIONAL_INFO(compositor_summed_area_table_compute_incomplete_prologues_shared)
DEFINE_VALUE("OPERATION(value)", "value")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_incomplete_prologues_square)
ADDITIONAL_INFO(compositor_summed_area_table_compute_incomplete_prologues_shared)
DEFINE_VALUE("OPERATION(value)", "value * value")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_complete_x_prologues)
LOCAL_GROUP_SIZE(16)
SAMPLER(0, sampler2D, incomplete_x_prologues_tx)
IMAGE(0, SFLOAT_32_32_32_32, write, image2D, complete_x_prologues_img)
IMAGE(1, SFLOAT_32_32_32_32, write, image2D, complete_x_prologues_sum_img)
COMPUTE_SOURCE("compositor_summed_area_table_compute_complete_x_prologues.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_complete_y_prologues)
LOCAL_GROUP_SIZE(16)
SAMPLER(0, sampler2D, incomplete_y_prologues_tx)
SAMPLER(1, sampler2D, complete_x_prologues_sum_tx)
IMAGE(0, SFLOAT_32_32_32_32, write, image2D, complete_y_prologues_img)
COMPUTE_SOURCE("compositor_summed_area_table_compute_complete_y_prologues.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_complete_blocks_shared)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
SAMPLER(1, sampler2D, complete_x_prologues_tx)
SAMPLER(2, sampler2D, complete_y_prologues_tx)
IMAGE(0, SFLOAT_32_32_32_32, read_write, image2D, output_img)
COMPUTE_SOURCE("compositor_summed_area_table_compute_complete_blocks.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_complete_blocks_identity)
ADDITIONAL_INFO(compositor_summed_area_table_compute_complete_blocks_shared)
DEFINE_VALUE("OPERATION(value)", "value")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_summed_area_table_compute_complete_blocks_square)
ADDITIONAL_INFO(compositor_summed_area_table_compute_complete_blocks_shared)
DEFINE_VALUE("OPERATION(value)", "value * value")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
