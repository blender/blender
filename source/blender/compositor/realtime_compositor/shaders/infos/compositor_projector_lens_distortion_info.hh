/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_projector_lens_distortion)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(FLOAT, dispersion)
SAMPLER(0, FLOAT_2D, input_tx)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_projector_lens_distortion.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
