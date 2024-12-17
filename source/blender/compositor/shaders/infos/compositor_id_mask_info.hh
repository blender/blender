/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_id_mask)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(INT, index)
SAMPLER(0, FLOAT_2D, input_mask_tx)
IMAGE(0, GPU_R16F, WRITE, FLOAT_2D, output_mask_img)
COMPUTE_SOURCE("compositor_id_mask.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
