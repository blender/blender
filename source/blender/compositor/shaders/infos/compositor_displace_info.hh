/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_displace)
LOCAL_GROUP_SIZE(16, 16)
SAMPLER(0, sampler2D, input_tx)
SAMPLER(1, sampler2D, displacement_tx)
SAMPLER(2, sampler2D, x_scale_tx)
SAMPLER(3, sampler2D, y_scale_tx)
IMAGE(0, GPU_RGBA16F, write, image2D, output_img)
COMPUTE_SOURCE("compositor_displace.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
