/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_shared)
LOCAL_GROUP_SIZE(16, 16)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_kuwahara_classic.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_convolution_shared)
ADDITIONAL_INFO(compositor_kuwahara_classic_shared)
SAMPLER(0, sampler2D, input_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_convolution_constant_size)
ADDITIONAL_INFO(compositor_kuwahara_classic_convolution_shared)
PUSH_CONSTANT(int, size)
DEFINE("CONSTANT_SIZE")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_convolution_variable_size)
ADDITIONAL_INFO(compositor_kuwahara_classic_convolution_shared)
SAMPLER(1, sampler2D, size_tx)
DEFINE("VARIABLE_SIZE")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_summed_area_table_shared)
ADDITIONAL_INFO(compositor_kuwahara_classic_shared)
DEFINE("SUMMED_AREA_TABLE")
SAMPLER(0, sampler2D, table_tx)
SAMPLER(1, sampler2D, squared_table_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_summed_area_table_constant_size)
ADDITIONAL_INFO(compositor_kuwahara_classic_summed_area_table_shared)
PUSH_CONSTANT(int, size)
DEFINE("CONSTANT_SIZE")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_kuwahara_classic_summed_area_table_variable_size)
ADDITIONAL_INFO(compositor_kuwahara_classic_summed_area_table_shared)
SAMPLER(2, sampler2D, size_tx)
DEFINE("VARIABLE_SIZE")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_kuwahara_anisotropic_compute_structure_tensor)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, structure_tensor_img)
COMPUTE_SOURCE("compositor_kuwahara_anisotropic_compute_structure_tensor.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_kuwahara_anisotropic_shared)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float, eccentricity)
PUSH_CONSTANT(float, sharpness)
SAMPLER(0, sampler2D, input_tx)
SAMPLER(1, sampler2D, structure_tensor_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_kuwahara_anisotropic.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_kuwahara_anisotropic_constant_size)
ADDITIONAL_INFO(compositor_kuwahara_anisotropic_shared)
DEFINE("CONSTANT_SIZE")
PUSH_CONSTANT(float, size)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_kuwahara_anisotropic_variable_size)
ADDITIONAL_INFO(compositor_kuwahara_anisotropic_shared)
DEFINE("VARIABLE_SIZE")
SAMPLER(2, sampler2D, size_tx)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
