/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

using namespace blender::gpu::shader;

GPU_SHADER_CREATE_INFO(compositor_cryptomatte_pick)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(int2, lower_bound)
SAMPLER(0, sampler2D, first_layer_tx)
IMAGE(0, SFLOAT_32_32_32_32, write, image2D, output_img)
COMPUTE_SOURCE("compositor_cryptomatte_pick.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_cryptomatte_matte)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(int2, lower_bound)
PUSH_CONSTANT(int, identifiers_count)
PUSH_CONSTANT_ARRAY(float, identifiers, 32)
SAMPLER(0, sampler2D, layer_tx)
IMAGE(0, SFLOAT_16, read_write, image2D, matte_img)
COMPUTE_SOURCE("compositor_cryptomatte_matte.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_cryptomatte_image)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
SAMPLER(1, sampler2D, matte_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_cryptomatte_image.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
