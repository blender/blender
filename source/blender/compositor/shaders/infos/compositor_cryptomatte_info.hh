/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

using namespace blender::gpu::shader;

GPU_SHADER_CREATE_INFO(compositor_cryptomatte_pick)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(IVEC2, lower_bound)
SAMPLER(0, FLOAT_2D, first_layer_tx)
IMAGE(0, GPU_RGBA32F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_cryptomatte_pick.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_cryptomatte_matte)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(IVEC2, lower_bound)
PUSH_CONSTANT(INT, identifiers_count)
PUSH_CONSTANT_ARRAY(FLOAT, identifiers, 32)
SAMPLER(0, FLOAT_2D, layer_tx)
IMAGE(0, GPU_R16F, READ_WRITE, FLOAT_2D, matte_img)
COMPUTE_SOURCE("compositor_cryptomatte_matte.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(compositor_cryptomatte_image)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, FLOAT_2D, input_tx)
SAMPLER(1, FLOAT_2D, matte_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_cryptomatte_image.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
